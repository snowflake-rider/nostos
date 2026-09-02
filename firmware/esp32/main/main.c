#include <stdio.h>
#include "esp_log.h"
#include "sdkconfig.h"

#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#error "Layer 8 requires USB Serial/JTAG as the primary console"
#endif
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_mesh_example_init.h"
#include "bridge_runtime.h"
#include "mesh_node.h"
#include "serial_command.h"
#if CONFIG_NOSTOS_XOSS_SPEED_SENSOR
#include "xoss_ble.h"
#define XOSS_ROLE_POLL_MS 100U
#endif

#if CONFIG_NOSTOS_XOSS_SPEED_SENSOR
static void xoss_role_init_task(void *argument)
{
    (void)argument;
    for (;;) {
        uint8_t source = bridge_runtime_local_source_node_id();
        if (source == 0U) {
            vTaskDelay(pdMS_TO_TICKS(XOSS_ROLE_POLL_MS));
            continue;
        }
        if (source != CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE) {
            ESP_LOGI("XOSS_ROLE", "DISABLED source=%u owner=%u",
                     (unsigned)source,
                     (unsigned)CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE);
            vTaskDelete(NULL);
            return;
        }

        esp_err_t err = xoss_ble_init();
        if (err != ESP_OK) {
            ESP_LOGE("XOSS_ROLE", "INIT_FAILED source=%u err=%s",
                     (unsigned)source, esp_err_to_name(err));
        } else {
            ESP_LOGI("XOSS_ROLE", "ENABLED source=%u", (unsigned)source);
        }
        vTaskDelete(NULL);
        return;
    }
}
#endif

static void console_task(void *argument)
{
    (void)argument;
    serial_command_parser_t parser;
    serial_command_t command;
    serial_command_parser_init(&parser);
    setvbuf(stdin, NULL, _IONBF, 0);
    ESP_LOGI("LAYER_8_CONSOLE", "commands=status,tx-low,tx-normal");
    for (;;) {
        int byte = getchar();
        if (byte == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!serial_command_parser_feed(&parser, (uint8_t)byte, &command)) continue;
        esp_err_t err = ESP_OK;
        switch (command) {
        case SERIAL_COMMAND_STATUS:
            mesh_node_log_status();
            bridge_runtime_log_status();
#if CONFIG_NOSTOS_XOSS_SPEED_SENSOR
            xoss_ble_log_status();
#endif
            break;
        case SERIAL_COMMAND_TX_LOW: err = mesh_node_set_low_tx_power(true); break;
        case SERIAL_COMMAND_TX_NORMAL: err = mesh_node_set_low_tx_power(false); break;
        case SERIAL_COMMAND_EMPTY: break;
        default: ESP_LOGW("LAYER_8_CONSOLE", "Command rejected/disabled"); break;
        }
        if (err != ESP_OK) ESP_LOGW("LAYER_8_CONSOLE", "API failed: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    ESP_LOGI("LAYER_8", "[LAYER-8] BOOT_START project=nostos_esp32");
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE("LAYER_8", "NVS init failed (%s). STOP: existing settings preserved; no automatic erase.", esp_err_to_name(err));
        return;
    }
    /* STM32 입력은 먼저 비운다. Mesh 설정 전의 메시지는 나중에 재생하지 않는다. */
    ESP_ERROR_CHECK(bridge_runtime_init());
    ESP_ERROR_CHECK(bluetooth_init());
    ESP_ERROR_CHECK(mesh_node_init());
#if CONFIG_NOSTOS_XOSS_SPEED_SENSOR
    if (xTaskCreate(xoss_role_init_task, "xoss_role", 2048U, NULL, 3U, NULL) != pdPASS) {
        ESP_LOGE("LAYER_8", "XOSS role task allocation failed");
        return;
    }
#endif
    if (xTaskCreate(console_task, "bsg_console", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE("LAYER_8", "Console task allocation failed");
        return;
    }
    ESP_LOGI("LAYER_8", "[LAYER-8] APP_STARTED; UART1 <-> Mesh C001; API acceptance is not peer delivery");
}
