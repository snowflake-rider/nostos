#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_mesh_example_init.h"
#include "bridge_runtime.h"
#include "mesh_node.h"
#include "serial_command.h"

static void console_task(void *argument)
{
    (void)argument;
    serial_command_parser_t parser;
    serial_command_t command;
    serial_command_parser_init(&parser);
    setvbuf(stdin, NULL, _IONBF, 0);
    ESP_LOGI("CONSOLE", "commands=status,on,off,on-unack,off-unack,tx-low,tx-normal; factory-reset DISABLED");
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
            break;
        case SERIAL_COMMAND_ON: err = mesh_node_send_onoff(true, true); break;
        case SERIAL_COMMAND_OFF: err = mesh_node_send_onoff(false, true); break;
        case SERIAL_COMMAND_ON_UNACK: err = mesh_node_send_onoff(true, false); break;
        case SERIAL_COMMAND_OFF_UNACK: err = mesh_node_send_onoff(false, false); break;
        case SERIAL_COMMAND_TX_LOW: err = mesh_node_set_low_tx_power(true); break;
        case SERIAL_COMMAND_TX_NORMAL: err = mesh_node_set_low_tx_power(false); break;
        case SERIAL_COMMAND_EMPTY: break;
        default: ESP_LOGW("CONSOLE", "Command rejected/disabled"); break;
        }
        if (err != ESP_OK) ESP_LOGW("CONSOLE", "API failed: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE("BSG", "NVS init failed (%s). STOP: existing settings preserved; no automatic erase.", esp_err_to_name(err));
        return;
    }
    /* Start input drain while Mesh is not ready; early UART events are discarded. */
    ESP_ERROR_CHECK(bridge_runtime_init());
    ESP_ERROR_CHECK(bluetooth_init());
    ESP_ERROR_CHECK(mesh_node_init());
    if (xTaskCreate(console_task, "bsg_console", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE("BSG", "Console task allocation failed");
        return;
    }
    ESP_LOGI("BSG", "APP_STARTED; transport acceptance is not peer delivery");
}
