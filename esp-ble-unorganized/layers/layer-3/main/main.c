/*
 * Layer 3: 가장 작은 BLE Active Scanner
 *
 * Board A의 ESP32-LAYER-2 Advertising + scan response를 Board B가
 * connection 없이 수신하고 이름, Service UUID, RSSI, count를 확인한다.
 * Advertising, GATT connection, Bluetooth Mesh, relay는 포함하지 않는다.
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define TAG "LAYER_3"
#define TARGET_NAME "ESP32-LAYER-2"
#define SCANNER_START_TIMEOUT_COUNT 100
#define TARGET_LOG_INTERVAL 10

/*
 * Scanner에는 7a110000-6b0d-4d5a-8f4b-2c9e00000001로 표시된다.
 * Advertising packet 내부 128-bit UUID는 little-endian byte order다.
 */
static const uint8_t target_service_uuid[ESP_UUID_LEN_128] = {
    0x01, 0x00, 0x00, 0x00, 0x9e, 0x2c, 0x4b, 0x8f,
    0x5a, 0x4d, 0x0d, 0x6b, 0x00, 0x00, 0x11, 0x7a,
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

static volatile bool scanner_started;
static volatile bool target_confirmed;
static volatile uint32_t target_receive_count;
static volatile int last_target_rssi;

static bool target_name_matches(uint8_t *advertising_bytes,
                                uint16_t advertising_length) {
  uint8_t name_length = 0;
  uint8_t *name = esp_ble_resolve_adv_data_by_type(
      advertising_bytes, advertising_length, ESP_BLE_AD_TYPE_NAME_CMPL,
      &name_length);

  if (name == NULL) {
    name = esp_ble_resolve_adv_data_by_type(
        advertising_bytes, advertising_length, ESP_BLE_AD_TYPE_NAME_SHORT,
        &name_length);
  }

  return name != NULL && name_length == sizeof(TARGET_NAME) - 1 &&
         memcmp(name, TARGET_NAME, sizeof(TARGET_NAME) - 1) == 0;
}

static bool target_service_matches(uint8_t *advertising_bytes,
                                   uint16_t advertising_length) {
  uint8_t service_length = 0;
  uint8_t *services = esp_ble_resolve_adv_data_by_type(
      advertising_bytes, advertising_length, ESP_BLE_AD_TYPE_128SRV_CMPL,
      &service_length);

  if (services == NULL) {
    services = esp_ble_resolve_adv_data_by_type(
        advertising_bytes, advertising_length, ESP_BLE_AD_TYPE_128SRV_PART,
        &service_length);
  }

  if (services == NULL || service_length < ESP_UUID_LEN_128) {
    return false;
  }

  for (uint16_t offset = 0; offset + ESP_UUID_LEN_128 <= service_length;
       offset += ESP_UUID_LEN_128) {
    if (memcmp(services + offset, target_service_uuid,
               ESP_UUID_LEN_128) == 0) {
      return true;
    }
  }
  return false;
}

static void handle_scan_result(esp_ble_gap_cb_param_t *param) {
  if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) {
    return;
  }

  const uint16_t advertising_length =
      (uint16_t)param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len;
  if (advertising_length == 0 ||
      advertising_length > sizeof(param->scan_rst.ble_adv)) {
    return;
  }

  if (!target_name_matches(param->scan_rst.ble_adv, advertising_length) ||
      !target_service_matches(param->scan_rst.ble_adv,
                              advertising_length)) {
    return;
  }

  const uint32_t count = ++target_receive_count;
  last_target_rssi = param->scan_rst.rssi;

  if (!target_confirmed) {
    target_confirmed = true;
    ESP_LOGI(TAG,
             "[LAYER-3] TARGET_FOUND name=%s rssi=%d "
             "peer=%02x:%02x:%02x:%02x:%02x:%02x",
             TARGET_NAME, param->scan_rst.rssi, param->scan_rst.bda[0],
             param->scan_rst.bda[1], param->scan_rst.bda[2],
             param->scan_rst.bda[3], param->scan_rst.bda[4],
             param->scan_rst.bda[5]);
    ESP_LOGI(TAG,
             "[LAYER-3] SERVICE_MATCH "
             "uuid=7A110000-6B0D-4D5A-8F4B-2C9E00000001");
    ESP_LOGI(TAG, "[LAYER-3] TARGET_RX count=%" PRIu32 " rssi=%d",
             count, param->scan_rst.rssi);
    ESP_LOGI(TAG, "[LAYER-3] SCAN_TARGET_CONFIRMED");
  } else if (count % TARGET_LOG_INTERVAL == 0) {
    ESP_LOGI(TAG, "[LAYER-3] TARGET_RX count=%" PRIu32 " rssi=%d",
             count, param->scan_rst.rssi);
  }
}

static void gap_callback(esp_gap_ble_cb_event_t event,
                         esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    if (param->scan_param_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-3] SCAN_PARAMS_FAILED status=%d",
               param->scan_param_cmpl.status);
      return;
    }

    ESP_LOGI(TAG, "[LAYER-3] SCAN_PARAMS_READY");
    const esp_err_t err = esp_ble_gap_start_scanning(0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "[LAYER-3] SCAN_START_REQUEST_FAILED err=%s",
               esp_err_to_name(err));
    }
    break;
  }

  case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
    if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      scanner_started = false;
      ESP_LOGE(TAG, "[LAYER-3] SCAN_START_FAILED status=%d",
               param->scan_start_cmpl.status);
      return;
    }
    scanner_started = true;
    ESP_LOGI(TAG, "[LAYER-3] SCANNING_STARTED mode=active");
    break;

  case ESP_GAP_BLE_SCAN_RESULT_EVT:
    handle_scan_result(param);
    break;

  case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
    scanner_started = false;
    ESP_LOGW(TAG, "[LAYER-3] SCANNING_STOPPED status=%d",
             param->scan_stop_cmpl.status);
    break;

  default:
    break;
  }
}

void app_main(void) {
  uint32_t wait_count = 0;
  uint32_t heartbeat_count = 0;

  ESP_LOGI(TAG, "[LAYER-3] BOOT_SUCCESS target=%s idf=%s",
           CONFIG_IDF_TARGET, esp_get_idf_version());

  /* Bluetooth 설정 저장소를 열되 기존 NVS를 자동으로 지우지 않는다. */
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_LOGI(TAG, "[LAYER-3] NVS_READY");

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_config_t controller_config =
      BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&controller_config));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_LOGI(TAG, "[LAYER-3] BLE_CONTROLLER_ENABLED");

  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  ESP_LOGI(TAG, "[LAYER-3] BLUEDROID_ENABLED");

  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_callback));
  ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));

  /* Scan parameter 설정과 scan start는 callback으로 완료된다. */
  while (!scanner_started && wait_count < SCANNER_START_TIMEOUT_COUNT) {
    wait_count++;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (!scanner_started) {
    ESP_LOGE(TAG, "[LAYER-3] SCANNER_START_TIMEOUT");
    return;
  }

  while (true) {
    heartbeat_count++;
    ESP_LOGI(TAG,
             "[LAYER-3] SCANNER_ACTIVE heartbeat=%" PRIu32
             " target_count=%" PRIu32 " last_rssi=%d confirmed=%s",
             heartbeat_count, (uint32_t)target_receive_count,
             (int)last_target_rssi, target_confirmed ? "yes" : "no");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
