/*
 * Layer 5: GATT Server + connectable Advertising + Active Scanning
 *
 * 두 ESP32-S3에 같은 firmware를 넣는다. 각 보드는 Bluetooth MAC으로
 * ESP32-L5-XX 이름을 만들고, GATT Server/Advertising을 제공하면서
 * 20-byte custom packet을 Advertising하고 다른 Layer 5 packet을 scan한다.
 * GATT Client, forwarding, relay, Bluetooth Mesh는 포함하지 않는다.
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "layer_packet.h"
#include "nvs_flash.h"

#define TAG "LAYER_5"
#define DEVICE_NAME_PREFIX "ESP32-L5-"
#define DEVICE_NAME_LENGTH 11
#define GATTS_APP_ID 0x44
#define SERVICE_INSTANCE_ID 0
#define PAYLOAD_MAX_LENGTH 20
#define DUAL_ROLE_START_TIMEOUT_COUNT 100
#define PACKET_RX_QUEUE_LENGTH 16
#define PACKET_ADV_DATA_LENGTH 27
#define PACKET_SCAN_RSP_LENGTH 31
#define PACKET_ADV_WIRE_OFFSET 7
#define PACKET_COMPANY_ID_LOW 0xFF
#define PACKET_COMPANY_ID_HIGH 0xFF

#define ADV_DATA_CONFIG_FLAG (1U << 0)
#define SCAN_RSP_CONFIG_FLAG (1U << 1)

enum layer_5_attribute_index {
  IDX_SERVICE,
  IDX_RX_CHAR_DECLARATION,
  IDX_RX_CHAR_VALUE,
  IDX_TX_CHAR_DECLARATION,
  IDX_TX_CHAR_VALUE,
  IDX_TX_CCCD,
  IDX_ATTRIBUTE_COUNT,
};

/* BLE packet 안의 128-bit UUID는 little-endian byte order다. */
static uint8_t service_uuid[ESP_UUID_LEN_128] = {
    0x01, 0x00, 0x00, 0x00, 0x9e, 0x2c, 0x4b, 0x8f,
    0x5a, 0x4d, 0x0d, 0x6b, 0x00, 0x00, 0x11, 0x7a,
};
static uint8_t rx_uuid[ESP_UUID_LEN_128] = {
    0x02, 0x00, 0x00, 0x00, 0x9e, 0x2c, 0x4b, 0x8f,
    0x5a, 0x4d, 0x0d, 0x6b, 0x00, 0x00, 0x11, 0x7a,
};
static uint8_t tx_uuid[ESP_UUID_LEN_128] = {
    0x03, 0x00, 0x00, 0x00, 0x9e, 0x2c, 0x4b, 0x8f,
    0x5a, 0x4d, 0x0d, 0x6b, 0x00, 0x00, 0x11, 0x7a,
};

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t rx_properties = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t tx_properties =
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t rx_initial_value[1] = {0};
static uint8_t tx_value[PAYLOAD_MAX_LENGTH] = "READY";
static uint16_t tx_value_length = 5;
static uint8_t cccd_initial_value[2] = {0x00, 0x00};

static uint16_t attribute_handles[IDX_ATTRIBUTE_COUNT];
static uint16_t active_connection_id;
static uint8_t local_bt_address[ESP_BD_ADDR_LEN];
static uint8_t local_node_id;
static char device_name[DEVICE_NAME_LENGTH + 1];

static volatile bool connected;
static volatile bool notification_enabled;
static volatile bool service_ready;
static volatile bool advertising_active;
static volatile bool scanner_started;
static volatile bool dual_role_ready_logged;
static volatile uint32_t packet_receive_count;
static volatile uint32_t packet_duplicate_count;
static volatile uint32_t packet_queue_drop_count;
static volatile int last_packet_rssi;
static volatile uint8_t last_packet_sender;

static bool advertising_start_requested;
static bool advertising_restart_pending;
static bool packet_update_pending;
static bool packet_tx_log_pending;
static uint8_t advertising_config_pending =
    ADV_DATA_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG;
static uint16_t packet_sequence = 1U;
static QueueHandle_t packet_rx_queue;
static layer_packet_dedup_t packet_dedup;

typedef struct {
  uint8_t wire[LAYER_PACKET_WIRE_SIZE];
  esp_bd_addr_t address;
  int rssi;
} packet_rx_record_t;

/* Primary ADV: Flags AD(3) + Manufacturer AD(24) = 27 bytes. */
static uint8_t raw_advertising_data[PACKET_ADV_DATA_LENGTH] = {
    0x02,
    ESP_BLE_AD_TYPE_FLAG,
    ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
    0x17,
    ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE,
    PACKET_COMPANY_ID_LOW,
    PACKET_COMPANY_ID_HIGH,
};

/* Scan response: Complete Name AD(13) + Complete 128-bit UUID AD(18) = 31. */
static uint8_t raw_scan_response_data[PACKET_SCAN_RSP_LENGTH];

static esp_ble_adv_params_t advertising_params = {
    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(160),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(320),
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* 0.625 ms unit: interval 160 = 100 ms, window 128 = 80 ms. */
static esp_ble_scan_params_t scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 160,
    .scan_window = 128,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

static const esp_gatts_attr_db_t gatt_database[IDX_ATTRIBUTE_COUNT] = {
    [IDX_SERVICE] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
          ESP_GATT_PERM_READ, sizeof(service_uuid), sizeof(service_uuid),
          service_uuid}},
    [IDX_RX_CHAR_DECLARATION] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
          ESP_GATT_PERM_READ, sizeof(rx_properties), sizeof(rx_properties),
          (uint8_t *)&rx_properties}},
    [IDX_RX_CHAR_VALUE] =
        {{ESP_GATT_RSP_BY_APP},
         {ESP_UUID_LEN_128, rx_uuid, ESP_GATT_PERM_WRITE,
          PAYLOAD_MAX_LENGTH, 0, rx_initial_value}},
    [IDX_TX_CHAR_DECLARATION] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
          ESP_GATT_PERM_READ, sizeof(tx_properties), sizeof(tx_properties),
          (uint8_t *)&tx_properties}},
    [IDX_TX_CHAR_VALUE] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_128, tx_uuid, ESP_GATT_PERM_READ,
          PAYLOAD_MAX_LENGTH, 5, tx_value}},
    [IDX_TX_CCCD] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&cccd_uuid,
          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
          sizeof(cccd_initial_value), sizeof(cccd_initial_value),
          cccd_initial_value}},
};

static void make_printable(const uint8_t *source, uint16_t source_length,
                           char *output) {
  uint16_t bounded_length = source_length;
  if (bounded_length > PAYLOAD_MAX_LENGTH) {
    bounded_length = PAYLOAD_MAX_LENGTH;
  }

  for (uint16_t index = 0; index < bounded_length; index++) {
    const uint8_t byte = source[index];
    output[index] = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
  }
  output[bounded_length] = '\0';
}

static esp_err_t prepare_advertising_packet(void) {
  static const uint8_t hello[] = {'H', 'E', 'L', 'L', 'O'};
  const layer_packet_t packet = {
      .version = LAYER_PACKET_VERSION,
      .type = LAYER_PACKET_TYPE_HELLO,
      .ttl = 2U,
      .sender = local_node_id,
      .recipient = LAYER_PACKET_BROADCAST,
      .sequence = packet_sequence,
      .payload_length = sizeof(hello),
      .payload = {'H', 'E', 'L', 'L', 'O'},
  };

  if (layer_packet_encode(&packet,
                          &raw_advertising_data[PACKET_ADV_WIRE_OFFSET]) !=
      LAYER_PACKET_OK) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

static void prepare_scan_response(void) {
  raw_scan_response_data[0] = DEVICE_NAME_LENGTH + 1U;
  raw_scan_response_data[1] = ESP_BLE_AD_TYPE_NAME_CMPL;
  memcpy(&raw_scan_response_data[2], device_name, DEVICE_NAME_LENGTH);
  raw_scan_response_data[13] = ESP_UUID_LEN_128 + 1U;
  raw_scan_response_data[14] = ESP_BLE_AD_TYPE_128SRV_CMPL;
  memcpy(&raw_scan_response_data[15], service_uuid, ESP_UUID_LEN_128);
}

/* GAP callback에서는 packet을 복사해 queue에 넣기만 한다. */
static void handle_scan_result(esp_ble_gap_cb_param_t *param) {
  if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT ||
      memcmp(param->scan_rst.bda, local_bt_address, ESP_BD_ADDR_LEN) == 0) {
    return;
  }

  const uint16_t advertising_length =
      (uint16_t)param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len;
  if (advertising_length == 0 ||
      advertising_length > sizeof(param->scan_rst.ble_adv)) {
    return;
  }

  uint8_t manufacturer_length = 0;
  uint8_t *manufacturer = esp_ble_resolve_adv_data_by_type(
      param->scan_rst.ble_adv, advertising_length,
      ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &manufacturer_length);
  if (manufacturer == NULL ||
      manufacturer_length != LAYER_PACKET_WIRE_SIZE + 2U ||
      manufacturer[0] != PACKET_COMPANY_ID_LOW ||
      manufacturer[1] != PACKET_COMPANY_ID_HIGH) {
    return;
  }

  packet_rx_record_t record = {.rssi = param->scan_rst.rssi};
  memcpy(record.wire, manufacturer + 2U, LAYER_PACKET_WIRE_SIZE);
  memcpy(record.address, param->scan_rst.bda, ESP_BD_ADDR_LEN);
  if (xQueueSend(packet_rx_queue, &record, 0) != pdTRUE) {
    packet_queue_drop_count++;
  }
}

static void packet_worker(void *argument) {
  (void)argument;
  packet_rx_record_t record;

  while (true) {
    if (xQueueReceive(packet_rx_queue, &record, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    layer_packet_t packet;
    const layer_packet_status_t status =
        layer_packet_decode(record.wire, &packet);
    if (status != LAYER_PACKET_OK) {
      ESP_LOGW(TAG, "[LAYER-5] PACKET_RX_REJECT status=%d rssi=%d",
               (int)status, record.rssi);
      continue;
    }
    if (packet.sender == local_node_id ||
        (packet.recipient != LAYER_PACKET_BROADCAST &&
         packet.recipient != local_node_id)) {
      continue;
    }
    if (layer_packet_dedup_is_duplicate_or_record(
            &packet_dedup, packet.sender, packet.sequence)) {
      packet_duplicate_count++;
      continue;
    }

    packet_receive_count++;
    last_packet_rssi = record.rssi;
    last_packet_sender = packet.sender;
    char payload[LAYER_PACKET_PAYLOAD_CAPACITY + 1U];
    make_printable(packet.payload, packet.payload_length, payload);
    ESP_LOGI(TAG,
             "[LAYER-5] PACKET_RX_NEW local=%02X sender=%02X seq=%u ttl=%u"
             " recipient=%02X len=%u payload=%s crc=ok rssi=%d"
             " peer=%02x:%02x:%02x:%02x:%02x:%02x",
             local_node_id, packet.sender, (unsigned)packet.sequence,
             (unsigned)packet.ttl, packet.recipient,
             (unsigned)packet.payload_length, payload, record.rssi,
             record.address[0], record.address[1], record.address[2],
             record.address[3], record.address[4], record.address[5]);
  }
}

static void maybe_log_dual_role_ready(void) {
  if (!dual_role_ready_logged && service_ready && advertising_active &&
      scanner_started) {
    dual_role_ready_logged = true;
    ESP_LOGI(TAG, "[LAYER-5] GATT_SERVER_READY");
    ESP_LOGI(TAG,
             "[LAYER-5] DUAL_ROLE_READY node=%02X name=%s adv=yes scan=yes",
             local_node_id, device_name);
  }
}

static void try_start_advertising(void) {
  if (!service_ready || advertising_config_pending != 0 || connected ||
      advertising_active || advertising_start_requested) {
    return;
  }

  const esp_err_t err = esp_ble_gap_start_advertising(&advertising_params);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-5] ADVERTISING_START_REQUEST_FAILED err=%s",
             esp_err_to_name(err));
    return;
  }
  advertising_start_requested = true;
}

static void request_packet_update(void) {
  if (connected || !advertising_active || packet_update_pending) {
    return;
  }

  packet_update_pending = true;
  const esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    packet_update_pending = false;
    ESP_LOGE(TAG, "[LAYER-5] PACKET_UPDATE_STOP_FAILED err=%s",
             esp_err_to_name(err));
  }
}

static void gap_callback(esp_gap_ble_cb_event_t event,
                         esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    if (param->adv_data_raw_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-5] ADV_DATA_FAILED status=%d",
               param->adv_data_raw_cmpl.status);
      return;
    }
    advertising_config_pending &= (uint8_t)~ADV_DATA_CONFIG_FLAG;
    try_start_advertising();
    break;

  case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
    if (param->scan_rsp_data_raw_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-5] SCAN_RSP_DATA_FAILED status=%d",
               param->scan_rsp_data_raw_cmpl.status);
      return;
    }
    advertising_config_pending &= (uint8_t)~SCAN_RSP_CONFIG_FLAG;
    if (advertising_config_pending == 0) {
      ESP_LOGI(TAG, "[LAYER-5] ADV_DATA_READY name=%s", device_name);
    }
    try_start_advertising();
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    advertising_start_requested = false;
    if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      advertising_active = false;
      ESP_LOGE(TAG, "[LAYER-5] ADVERTISING_START_FAILED status=%d",
               param->adv_start_cmpl.status);
      return;
    }
    advertising_active = true;
    ESP_LOGI(TAG,
             "[LAYER-5] ADVERTISING_STARTED node=%02X name=%s type=connectable",
             local_node_id, device_name);
    if (packet_tx_log_pending) {
      packet_tx_log_pending = false;
      ESP_LOGI(TAG,
               "[LAYER-5] PACKET_TX sender=%02X seq=%u ttl=2 recipient=FF"
               " len=5 payload=HELLO crc=ok",
               local_node_id, (unsigned)packet_sequence);
    }
    packet_update_pending = false;
    if (advertising_restart_pending) {
      advertising_restart_pending = false;
      ESP_LOGI(TAG, "[LAYER-5] ADVERTISING_RESTARTED");
    }
    maybe_log_dual_role_ready();
    break;

  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    advertising_active = false;
    ESP_LOGI(TAG, "[LAYER-5] ADVERTISING_STOPPED status=%d",
             param->adv_stop_cmpl.status);
    if (packet_update_pending &&
        param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
      packet_sequence++;
      if (packet_sequence == 0U) {
        packet_sequence = 1U;
      }
      if (prepare_advertising_packet() != ESP_OK) {
        packet_update_pending = false;
        ESP_LOGE(TAG, "[LAYER-5] PACKET_ENCODE_FAILED");
        break;
      }
      advertising_config_pending |= ADV_DATA_CONFIG_FLAG;
      packet_tx_log_pending = true;
      const esp_err_t err = esp_ble_gap_config_adv_data_raw(
          raw_advertising_data, sizeof(raw_advertising_data));
      if (err != ESP_OK) {
        advertising_config_pending &= (uint8_t)~ADV_DATA_CONFIG_FLAG;
        packet_update_pending = false;
        packet_tx_log_pending = false;
        ESP_LOGE(TAG, "[LAYER-5] PACKET_CONFIG_FAILED err=%s",
                 esp_err_to_name(err));
      }
    } else if (packet_update_pending) {
      packet_update_pending = false;
      ESP_LOGE(TAG, "[LAYER-5] PACKET_UPDATE_STOP_STATUS_FAILED");
    }
    break;

  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    if (param->scan_param_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-5] SCAN_PARAMS_FAILED status=%d",
               param->scan_param_cmpl.status);
      return;
    }
    ESP_LOGI(TAG, "[LAYER-5] SCAN_PARAMS_READY");
    const esp_err_t err = esp_ble_gap_start_scanning(0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "[LAYER-5] SCAN_START_REQUEST_FAILED err=%s",
               esp_err_to_name(err));
    }
    break;
  }

  case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
    if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      scanner_started = false;
      ESP_LOGE(TAG, "[LAYER-5] SCAN_START_FAILED status=%d",
               param->scan_start_cmpl.status);
      return;
    }
    scanner_started = true;
    ESP_LOGI(TAG, "[LAYER-5] SCANNING_STARTED mode=active");
    maybe_log_dual_role_ready();
    break;

  case ESP_GAP_BLE_SCAN_RESULT_EVT:
    handle_scan_result(param);
    break;

  case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
    scanner_started = false;
    ESP_LOGW(TAG, "[LAYER-5] SCANNING_STOPPED status=%d",
             param->scan_stop_cmpl.status);
    break;

  default:
    break;
  }
}

static void send_rx_write_response(esp_gatt_if_t gatts_if,
                                   esp_ble_gatts_cb_param_t *param,
                                   esp_gatt_status_t status) {
  if (!param->write.need_rsp) {
    return;
  }
  const esp_err_t err = esp_ble_gatts_send_response(
      gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-5] WRITE_RESPONSE_FAILED err=%s",
             esp_err_to_name(err));
  }
}

static void handle_rx_write(esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t *param) {
  if (param->write.is_prep) {
    ESP_LOGW(TAG, "[LAYER-5] RX_WRITE_REJECTED reason=prepare-write");
    send_rx_write_response(gatts_if, param, ESP_GATT_NOT_LONG);
    return;
  }
  if (param->write.len == 0 || param->write.len > PAYLOAD_MAX_LENGTH) {
    ESP_LOGW(TAG, "[LAYER-5] RX_WRITE_REJECTED len=%u",
             (unsigned)param->write.len);
    send_rx_write_response(gatts_if, param, ESP_GATT_INVALID_ATTR_LEN);
    return;
  }

  char printable_rx[PAYLOAD_MAX_LENGTH + 1];
  make_printable(param->write.value, param->write.len, printable_rx);

  static const uint8_t ack_prefix[] = {'A', 'C', 'K', ':'};
  const uint16_t copied_length =
      param->write.len < (PAYLOAD_MAX_LENGTH - sizeof(ack_prefix))
          ? param->write.len
          : (PAYLOAD_MAX_LENGTH - sizeof(ack_prefix));
  memcpy(tx_value, ack_prefix, sizeof(ack_prefix));
  memcpy(tx_value + sizeof(ack_prefix), param->write.value, copied_length);
  tx_value_length = (uint16_t)(sizeof(ack_prefix) + copied_length);

  const esp_err_t set_value_err = esp_ble_gatts_set_attr_value(
      attribute_handles[IDX_TX_CHAR_VALUE], tx_value_length, tx_value);
  if (set_value_err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-5] TX_VALUE_UPDATE_FAILED err=%s",
             esp_err_to_name(set_value_err));
    send_rx_write_response(gatts_if, param, ESP_GATT_ERROR);
    return;
  }

  ESP_LOGI(TAG, "[LAYER-5] RX_WRITE len=%u value=%s",
           (unsigned)param->write.len, printable_rx);
  send_rx_write_response(gatts_if, param, ESP_GATT_OK);

  char printable_tx[PAYLOAD_MAX_LENGTH + 1];
  make_printable(tx_value, tx_value_length, printable_tx);
  if (!connected || !notification_enabled) {
    ESP_LOGI(TAG,
             "[LAYER-5] TX_UPDATED_NOTIFY_DISABLED len=%u value=%s",
             (unsigned)tx_value_length, printable_tx);
    return;
  }

  const esp_err_t notify_err = esp_ble_gatts_send_indicate(
      gatts_if, active_connection_id, attribute_handles[IDX_TX_CHAR_VALUE],
      tx_value_length, tx_value, false);
  if (notify_err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-5] TX_NOTIFY_FAILED err=%s",
             esp_err_to_name(notify_err));
    return;
  }
  ESP_LOGI(TAG, "[LAYER-5] TX_NOTIFY len=%u value=%s",
           (unsigned)tx_value_length, printable_tx);
}

static void handle_cccd_write(const esp_ble_gatts_cb_param_t *param) {
  if (param->write.is_prep || param->write.len != 2) {
    ESP_LOGW(TAG, "[LAYER-5] CCCD_WRITE_IGNORED len=%u",
             (unsigned)param->write.len);
    return;
  }
  const uint16_t cccd_value =
      (uint16_t)param->write.value[0] |
      ((uint16_t)param->write.value[1] << 8);
  notification_enabled = cccd_value == 0x0001;
  ESP_LOGI(TAG, "[LAYER-5] NOTIFY_%s",
           notification_enabled ? "ENABLED" : "DISABLED");
}

static void gatts_callback(esp_gatts_cb_event_t event,
                           esp_gatt_if_t gatts_if,
                           esp_ble_gatts_cb_param_t *param) {
  switch (event) {
  case ESP_GATTS_REG_EVT:
    if (param->reg.status != ESP_GATT_OK) {
      ESP_LOGE(TAG, "[LAYER-5] GATTS_REGISTER_FAILED status=%d",
               param->reg.status);
      return;
    }
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(device_name));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(
        raw_advertising_data, sizeof(raw_advertising_data)));
    ESP_ERROR_CHECK(esp_ble_gap_config_scan_rsp_data_raw(
        raw_scan_response_data, sizeof(raw_scan_response_data)));
    ESP_ERROR_CHECK(esp_ble_gatts_create_attr_tab(
        gatt_database, gatts_if, IDX_ATTRIBUTE_COUNT, SERVICE_INSTANCE_ID));
    break;

  case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    if (param->add_attr_tab.status != ESP_GATT_OK ||
        param->add_attr_tab.num_handle != IDX_ATTRIBUTE_COUNT) {
      ESP_LOGE(TAG,
               "[LAYER-5] GATT_TABLE_FAILED status=%d handles=%u expected=%u",
               param->add_attr_tab.status,
               (unsigned)param->add_attr_tab.num_handle,
               (unsigned)IDX_ATTRIBUTE_COUNT);
      return;
    }
    memcpy(attribute_handles, param->add_attr_tab.handles,
           sizeof(attribute_handles));
    ESP_ERROR_CHECK(
        esp_ble_gatts_start_service(attribute_handles[IDX_SERVICE]));
    break;

  case ESP_GATTS_START_EVT:
    if (param->start.status != ESP_GATT_OK) {
      ESP_LOGE(TAG, "[LAYER-5] GATT_SERVICE_START_FAILED status=%d",
               param->start.status);
      return;
    }
    service_ready = true;
    ESP_LOGI(TAG, "[LAYER-5] GATT_SERVICE_READY");
    try_start_advertising();
    maybe_log_dual_role_ready();
    break;

  case ESP_GATTS_CONNECT_EVT:
    connected = true;
    advertising_active = false;
    advertising_start_requested = false;
    notification_enabled = false;
    active_connection_id = param->connect.conn_id;
    ESP_LOGI(TAG,
             "[LAYER-5] CONNECTED conn_id=%u peer=%02x:%02x:%02x:%02x:%02x:%02x scan=%s",
             (unsigned)active_connection_id, param->connect.remote_bda[0],
             param->connect.remote_bda[1], param->connect.remote_bda[2],
             param->connect.remote_bda[3], param->connect.remote_bda[4],
             param->connect.remote_bda[5], scanner_started ? "active" : "stopped");
    break;

  case ESP_GATTS_DISCONNECT_EVT:
    connected = false;
    notification_enabled = false;
    advertising_active = false;
    advertising_start_requested = false;
    ESP_LOGI(TAG, "[LAYER-5] DISCONNECTED reason=0x%02x scan=%s",
             param->disconnect.reason,
             scanner_started ? "active" : "stopped");
    advertising_restart_pending = true;
    try_start_advertising();
    break;

  case ESP_GATTS_WRITE_EVT:
    if (param->write.handle == attribute_handles[IDX_RX_CHAR_VALUE]) {
      handle_rx_write(gatts_if, param);
    } else if (param->write.handle == attribute_handles[IDX_TX_CCCD]) {
      handle_cccd_write(param);
    }
    break;

  case ESP_GATTS_READ_EVT:
    if (param->read.handle == attribute_handles[IDX_TX_CHAR_VALUE]) {
      char printable_tx[PAYLOAD_MAX_LENGTH + 1];
      make_printable(tx_value, tx_value_length, printable_tx);
      ESP_LOGI(TAG, "[LAYER-5] TX_READ len=%u value=%s",
               (unsigned)tx_value_length, printable_tx);
    }
    break;

  case ESP_GATTS_MTU_EVT:
    ESP_LOGI(TAG, "[LAYER-5] MTU_UPDATED mtu=%u", (unsigned)param->mtu.mtu);
    break;

  case ESP_GATTS_CONF_EVT:
    ESP_LOGI(TAG, "[LAYER-5] INDICATION_CONFIRMED status=%d",
             param->conf.status);
    break;

  default:
    break;
  }
}

void app_main(void) {
  uint32_t wait_count = 0;
  uint32_t heartbeat_count = 0;

  ESP_LOGI(TAG, "[LAYER-5] BOOT_SUCCESS target=%s idf=%s",
           CONFIG_IDF_TARGET, esp_get_idf_version());
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_LOGI(TAG, "[LAYER-5] NVS_READY");

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  esp_bt_controller_config_t controller_config =
      BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&controller_config));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_LOGI(TAG, "[LAYER-5] BLE_CONTROLLER_ENABLED");

  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  ESP_LOGI(TAG, "[LAYER-5] BLUEDROID_ENABLED");

  ESP_ERROR_CHECK(esp_read_mac(local_bt_address, ESP_MAC_BT));
  local_node_id = local_bt_address[ESP_BD_ADDR_LEN - 1];
  const int written = snprintf(device_name, sizeof(device_name),
                               DEVICE_NAME_PREFIX "%02X", local_node_id);
  if (written != DEVICE_NAME_LENGTH) {
    ESP_LOGE(TAG, "[LAYER-5] DEVICE_NAME_FAILED length=%d", written);
    return;
  }
  ESP_LOGI(TAG,
           "[LAYER-5] NODE_READY id=%02X name=%s mac=%02x:%02x:%02x:%02x:%02x:%02x",
           local_node_id, device_name, local_bt_address[0], local_bt_address[1],
           local_bt_address[2], local_bt_address[3], local_bt_address[4],
           local_bt_address[5]);

  ESP_ERROR_CHECK(prepare_advertising_packet());
  prepare_scan_response();
  packet_tx_log_pending = true;
  layer_packet_dedup_init(&packet_dedup);
  packet_rx_queue = xQueueCreate(PACKET_RX_QUEUE_LENGTH,
                                 sizeof(packet_rx_record_t));
  if (packet_rx_queue == NULL) {
    ESP_LOGE(TAG, "[LAYER-5] PACKET_QUEUE_CREATE_FAILED");
    return;
  }
  if (xTaskCreate(packet_worker, "layer5_packet_rx", 4096, NULL, 5, NULL) !=
      pdPASS) {
    ESP_LOGE(TAG, "[LAYER-5] PACKET_WORKER_CREATE_FAILED");
    return;
  }

  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_callback));
  ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_callback));
  ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
  ESP_ERROR_CHECK(esp_ble_gatts_app_register(GATTS_APP_ID));

  while ((!service_ready || !advertising_active || !scanner_started) &&
         wait_count < DUAL_ROLE_START_TIMEOUT_COUNT) {
    wait_count++;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (!service_ready || !advertising_active || !scanner_started) {
    ESP_LOGE(TAG,
             "[LAYER-5] DUAL_ROLE_START_TIMEOUT gatt=%s advertising=%s scanning=%s",
             service_ready ? "yes" : "no",
             advertising_active ? "yes" : "no",
             scanner_started ? "yes" : "no");
    return;
  }
  maybe_log_dual_role_ready();

  while (true) {
    heartbeat_count++;
    ESP_LOGI(TAG,
             "[LAYER-5] PACKET_NODE_ACTIVE heartbeat=%" PRIu32
             " node=%02X gatt=%s advertising=%s scanning=%s connected=%s"
             " tx_seq=%u rx_new=%" PRIu32 " duplicates=%" PRIu32
             " queue_drops=%" PRIu32 " last_sender=%02X last_rssi=%d",
             heartbeat_count, local_node_id, service_ready ? "yes" : "no",
             advertising_active ? "yes" : "no",
             scanner_started ? "yes" : "no", connected ? "yes" : "no",
             (unsigned)packet_sequence, (uint32_t)packet_receive_count,
             (uint32_t)packet_duplicate_count,
             (uint32_t)packet_queue_drop_count,
             (uint8_t)last_packet_sender, (int)last_packet_rssi);
    request_packet_update();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
