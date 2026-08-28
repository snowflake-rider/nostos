/*
 * Layer 2: 가장 작은 connectable BLE GATT Server
 *
 * Phone -> RX Write("HELLO") -> ESP32 -> TX Read/Notify("ACK:HELLO")
 * Scanning, GATT Client, pairing, Bluetooth Mesh, relay는 포함하지 않는다.
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define TAG "LAYER_2"
#define DEVICE_NAME "ESP32-LAYER-2"
#define GATTS_APP_ID 0x42
#define SERVICE_INSTANCE_ID 0
#define PAYLOAD_MAX_LENGTH 20
#define SERVER_START_TIMEOUT_COUNT 100

#define ADV_DATA_CONFIG_FLAG (1U << 0)
#define SCAN_RSP_CONFIG_FLAG (1U << 1)

enum layer_2_attribute_index {
  IDX_SERVICE,
  IDX_RX_CHAR_DECLARATION,
  IDX_RX_CHAR_VALUE,
  IDX_TX_CHAR_DECLARATION,
  IDX_TX_CHAR_VALUE,
  IDX_TX_CCCD,
  IDX_ATTRIBUTE_COUNT,
};

/*
 * BLE packet에는 128-bit UUID가 little-endian byte order로 들어간다.
 * Scanner에는 다음 UUID로 표시된다.
 *   Service: 7a110000-6b0d-4d5a-8f4b-2c9e00000001
 *   RX:      7a110000-6b0d-4d5a-8f4b-2c9e00000002
 *   TX:      7a110000-6b0d-4d5a-8f4b-2c9e00000003
 */
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
static volatile bool connected;
static volatile bool notification_enabled;
static volatile bool service_ready;
static volatile bool advertising_active;
static volatile bool gatt_server_ready;
static bool advertising_start_requested;
static bool advertising_restart_pending;
static uint8_t advertising_config_pending =
    ADV_DATA_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG;

/* Service UUID는 Advertising packet, 긴 device name은 scan response에 넣는다. */
static esp_ble_adv_data_t advertising_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_data_t scan_response_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = false,
};

static esp_ble_adv_params_t advertising_params = {
    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(160),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(320),
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* RX는 application이 길이를 검사한 뒤 response를 직접 보낸다. */
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
                           char output[PAYLOAD_MAX_LENGTH + 1]) {
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

static void try_start_advertising(void) {
  if (!service_ready || advertising_config_pending != 0 || connected ||
      advertising_active || advertising_start_requested) {
    return;
  }

  const esp_err_t err = esp_ble_gap_start_advertising(&advertising_params);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-2] ADVERTISING_START_REQUEST_FAILED err=%s",
             esp_err_to_name(err));
    return;
  }
  advertising_start_requested = true;
}

static void gap_callback(esp_gap_ble_cb_event_t event,
                         esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    if (param->adv_data_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-2] ADV_DATA_FAILED status=%d",
               param->adv_data_cmpl.status);
      return;
    }
    advertising_config_pending &= (uint8_t)~ADV_DATA_CONFIG_FLAG;
    if (advertising_config_pending == 0) {
      ESP_LOGI(TAG, "[LAYER-2] ADV_DATA_READY name=%s", DEVICE_NAME);
    }
    try_start_advertising();
    break;

  case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    if (param->scan_rsp_data_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-2] SCAN_RSP_DATA_FAILED status=%d",
               param->scan_rsp_data_cmpl.status);
      return;
    }
    advertising_config_pending &= (uint8_t)~SCAN_RSP_CONFIG_FLAG;
    if (advertising_config_pending == 0) {
      ESP_LOGI(TAG, "[LAYER-2] ADV_DATA_READY name=%s", DEVICE_NAME);
    }
    try_start_advertising();
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    advertising_start_requested = false;
    if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      advertising_active = false;
      ESP_LOGE(TAG, "[LAYER-2] ADVERTISING_START_FAILED status=%d",
               param->adv_start_cmpl.status);
      return;
    }

    advertising_active = true;
    gatt_server_ready = true;
    ESP_LOGI(TAG,
             "[LAYER-2] ADVERTISING_STARTED name=%s type=connectable",
             DEVICE_NAME);
    if (advertising_restart_pending) {
      advertising_restart_pending = false;
      ESP_LOGI(TAG, "[LAYER-2] ADVERTISING_RESTARTED");
    }
    break;

  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    advertising_active = false;
    ESP_LOGI(TAG, "[LAYER-2] ADVERTISING_STOPPED status=%d",
             param->adv_stop_cmpl.status);
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
    ESP_LOGE(TAG, "[LAYER-2] WRITE_RESPONSE_FAILED err=%s",
             esp_err_to_name(err));
  }
}

static void handle_rx_write(esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t *param) {
  if (param->write.is_prep) {
    ESP_LOGW(TAG, "[LAYER-2] RX_WRITE_REJECTED reason=prepare-write");
    send_rx_write_response(gatts_if, param, ESP_GATT_NOT_LONG);
    return;
  }

  if (param->write.len == 0 || param->write.len > PAYLOAD_MAX_LENGTH) {
    ESP_LOGW(TAG, "[LAYER-2] RX_WRITE_REJECTED len=%u",
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
    ESP_LOGE(TAG, "[LAYER-2] TX_VALUE_UPDATE_FAILED err=%s",
             esp_err_to_name(set_value_err));
    send_rx_write_response(gatts_if, param, ESP_GATT_ERROR);
    return;
  }

  ESP_LOGI(TAG, "[LAYER-2] RX_WRITE len=%u value=%s",
           (unsigned)param->write.len, printable_rx);
  send_rx_write_response(gatts_if, param, ESP_GATT_OK);

  char printable_tx[PAYLOAD_MAX_LENGTH + 1];
  make_printable(tx_value, tx_value_length, printable_tx);

  if (!connected || !notification_enabled) {
    ESP_LOGI(TAG,
             "[LAYER-2] TX_UPDATED_NOTIFY_DISABLED len=%u value=%s",
             (unsigned)tx_value_length, printable_tx);
    return;
  }

  const esp_err_t notify_err = esp_ble_gatts_send_indicate(
      gatts_if, active_connection_id, attribute_handles[IDX_TX_CHAR_VALUE],
      tx_value_length, tx_value, false);
  if (notify_err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-2] TX_NOTIFY_FAILED err=%s",
             esp_err_to_name(notify_err));
    return;
  }

  ESP_LOGI(TAG, "[LAYER-2] TX_NOTIFY len=%u value=%s",
           (unsigned)tx_value_length, printable_tx);
}

static void handle_cccd_write(const esp_ble_gatts_cb_param_t *param) {
  if (param->write.is_prep || param->write.len != 2) {
    ESP_LOGW(TAG, "[LAYER-2] CCCD_WRITE_IGNORED len=%u",
             (unsigned)param->write.len);
    return;
  }

  const uint16_t cccd_value =
      (uint16_t)param->write.value[0] |
      ((uint16_t)param->write.value[1] << 8);
  notification_enabled = cccd_value == 0x0001;
  ESP_LOGI(TAG, "[LAYER-2] NOTIFY_%s",
           notification_enabled ? "ENABLED" : "DISABLED");
}

static void gatts_callback(esp_gatts_cb_event_t event,
                           esp_gatt_if_t gatts_if,
                           esp_ble_gatts_cb_param_t *param) {
  switch (event) {
  case ESP_GATTS_REG_EVT: {
    if (param->reg.status != ESP_GATT_OK) {
      ESP_LOGE(TAG, "[LAYER-2] GATTS_REGISTER_FAILED status=%d",
               param->reg.status);
      return;
    }

    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&advertising_data));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&scan_response_data));
    ESP_ERROR_CHECK(esp_ble_gatts_create_attr_tab(
        gatt_database, gatts_if, IDX_ATTRIBUTE_COUNT, SERVICE_INSTANCE_ID));
    break;
  }

  case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    if (param->add_attr_tab.status != ESP_GATT_OK ||
        param->add_attr_tab.num_handle != IDX_ATTRIBUTE_COUNT) {
      ESP_LOGE(TAG,
               "[LAYER-2] GATT_TABLE_FAILED status=%d handles=%u expected=%u",
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
      ESP_LOGE(TAG, "[LAYER-2] GATT_SERVICE_START_FAILED status=%d",
               param->start.status);
      return;
    }
    service_ready = true;
    ESP_LOGI(TAG, "[LAYER-2] GATT_SERVICE_READY");
    try_start_advertising();
    break;

  case ESP_GATTS_CONNECT_EVT:
    connected = true;
    advertising_active = false;
    advertising_start_requested = false;
    notification_enabled = false;
    active_connection_id = param->connect.conn_id;
    ESP_LOGI(TAG,
             "[LAYER-2] CONNECTED conn_id=%u peer=%02x:%02x:%02x:%02x:%02x:%02x",
             (unsigned)active_connection_id, param->connect.remote_bda[0],
             param->connect.remote_bda[1], param->connect.remote_bda[2],
             param->connect.remote_bda[3], param->connect.remote_bda[4],
             param->connect.remote_bda[5]);
    break;

  case ESP_GATTS_DISCONNECT_EVT:
    connected = false;
    notification_enabled = false;
    advertising_active = false;
    advertising_start_requested = false;
    ESP_LOGI(TAG, "[LAYER-2] DISCONNECTED reason=0x%02x",
             param->disconnect.reason);
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
      ESP_LOGI(TAG, "[LAYER-2] TX_READ len=%u value=%s",
               (unsigned)tx_value_length, printable_tx);
    }
    break;

  case ESP_GATTS_MTU_EVT:
    ESP_LOGI(TAG, "[LAYER-2] MTU_UPDATED mtu=%u", (unsigned)param->mtu.mtu);
    break;

  case ESP_GATTS_CONF_EVT:
    ESP_LOGI(TAG, "[LAYER-2] INDICATION_CONFIRMED status=%d",
             param->conf.status);
    break;

  default:
    break;
  }
}

void app_main(void) {
  uint32_t wait_count = 0;
  uint32_t active_count = 0;

  ESP_LOGI(TAG, "[LAYER-2] BOOT_SUCCESS target=%s idf=%s",
           CONFIG_IDF_TARGET, esp_get_idf_version());

  /* Bluetooth 설정 저장소를 열되 기존 NVS를 자동으로 지우지 않는다. */
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_LOGI(TAG, "[LAYER-2] NVS_READY");

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_config_t controller_config =
      BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&controller_config));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_LOGI(TAG, "[LAYER-2] BLE_CONTROLLER_ENABLED");

  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  ESP_LOGI(TAG, "[LAYER-2] BLUEDROID_ENABLED");

  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_callback));
  ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_callback));
  ESP_ERROR_CHECK(esp_ble_gatts_app_register(GATTS_APP_ID));

  /* Service table과 Advertising은 callback으로 완료되므로 최대 10초 기다린다. */
  while (!gatt_server_ready && wait_count < SERVER_START_TIMEOUT_COUNT) {
    wait_count++;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (!gatt_server_ready) {
    ESP_LOGE(TAG, "[LAYER-2] GATT_SERVER_START_TIMEOUT");
    return;
  }

  ESP_LOGI(TAG, "[LAYER-2] GATT_SERVER_READY");
  while (true) {
    active_count++;
    ESP_LOGI(TAG,
             "[LAYER-2] GATT_SERVER_ACTIVE count=%" PRIu32
             " connected=%s notify=%s",
             active_count, connected ? "yes" : "no",
             notification_enabled ? "yes" : "no");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
