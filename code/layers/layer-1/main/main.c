/*
 * Layer 1: BLE Controller와 Bluedroid Host를 켜고 이름을 Advertising한다.
 * 연결, GATT, Scanning, Bluetooth Mesh는 의도적으로 넣지 않는다.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define TAG "LAYER_1"
#define DEVICE_NAME "ESP32-LAYER-1"
#define ADVERTISING_START_TIMEOUT_COUNT 100

/* GAP callback이 Advertising 시작 성공을 알리면 app_main()이 이 값을 읽는다. */
static volatile bool advertising_started = false;

/* 이름과 BLE discovery flag만 넣는 가장 작은 구조화 Advertising data다. */
static esp_ble_adv_data_t advertising_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

/* 연결을 받지 않고 160~320 ms 간격으로 세 Advertising channel에 전송한다. */
static esp_ble_adv_params_t advertising_params = {
    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(160),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(320),
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_callback(esp_gap_ble_cb_event_t event,
                         esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
    if (param->adv_data_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "[LAYER-1] ADV_DATA_FAILED status=%d",
               param->adv_data_cmpl.status);
      return;
    }

    ESP_LOGI(TAG, "[LAYER-1] ADV_DATA_READY name=%s", DEVICE_NAME);
    esp_err_t err = esp_ble_gap_start_advertising(&advertising_params);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "[LAYER-1] ADVERTISING_START_REQUEST_FAILED err=%s",
               esp_err_to_name(err));
    }
    break;
  }

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
      advertising_started = true;
      ESP_LOGI(TAG,
               "[LAYER-1] ADVERTISING_STARTED name=%s type=non-connectable",
               DEVICE_NAME);
    } else {
      advertising_started = false;
      ESP_LOGE(TAG, "[LAYER-1] ADVERTISING_START_FAILED status=%d",
               param->adv_start_cmpl.status);
    }
    break;

  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    advertising_started = false;
    ESP_LOGW(TAG, "[LAYER-1] ADVERTISING_STOPPED status=%d",
             param->adv_stop_cmpl.status);
    break;

  default:
    break;
  }
}

void app_main(void) {
  uint32_t wait_count = 0;
  uint32_t active_count = 0;

  ESP_LOGI(TAG, "[LAYER-1] BOOT_SUCCESS target=%s idf=%s",
           CONFIG_IDF_TARGET, esp_get_idf_version());

  /* Bluetooth calibration/configuration storage를 열되 기존 NVS를 지우지 않는다. */
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_LOGI(TAG, "[LAYER-1] NVS_READY");

  /* ESP32-S3는 BLE-only이므로 사용하지 않는 Classic BT memory를 반환한다. */
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_config_t controller_config =
      BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&controller_config));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_LOGI(TAG, "[LAYER-1] BLE_CONTROLLER_ENABLED");

  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  ESP_LOGI(TAG, "[LAYER-1] BLUEDROID_ENABLED");

  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_callback));
  ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
  ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&advertising_data));

  /* Advertising 설정과 시작은 callback으로 완료되므로 최대 10초 기다린다. */
  while (!advertising_started &&
         wait_count < ADVERTISING_START_TIMEOUT_COUNT) {
    wait_count++;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (!advertising_started) {
    ESP_LOGE(TAG, "[LAYER-1] ADVERTISING_START_TIMEOUT");
    return;
  }

  while (true) {
    active_count++;
    ESP_LOGI(TAG, "[LAYER-1] ADVERTISING_ACTIVE count=%" PRIu32,
             active_count);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
