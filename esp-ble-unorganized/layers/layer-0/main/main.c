/*
 * Layer 0: ESP32-S3가 새 application의 app_main()까지 실행했는지 확인한다.
 * BLE, Wi-Fi, sensor 같은 다음 단계 기능은 의도적으로 넣지 않는다.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_idf_version.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "LAYER_0"

void app_main(void) {
  uint32_t alive_count = 0;

  /* 이 줄이 보이면 ROM bootloader -> flash application -> app_main 진입이 끝났다. */
  ESP_LOGI(TAG, "[LAYER-0] BOOT_SUCCESS target=%s idf=%s",
           CONFIG_IDF_TARGET, esp_get_idf_version());

  while (true) {
    /* bootload.sh가 serial port를 늦게 열어도 실행 중임을 확인할 수 있다. */
    alive_count++;
    ESP_LOGI(TAG, "[LAYER-0] RUNTIME_OK count=%" PRIu32, alive_count);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
