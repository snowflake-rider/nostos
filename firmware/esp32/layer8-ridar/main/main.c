#include <stdio.h>

#include "ble_mesh_example_init.h"
#include "bridge_runtime.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lidar_c1.h"
#include "mesh_node.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "serial_command.h"

#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#error "Layer 8 RIDAR requires USB Serial/JTAG as the primary console"
#endif

static void log_help(void)
{
    ESP_LOGI("LAYER8_RIDAR_CONSOLE",
             "commands=help,status,lidar-status,lidar-info,lidar-health,"
             "lidar-rx-test,"
             "on,off,on-unack,off-unack,tx-low,tx-normal; "
             "factory-reset and lidar scan DISABLED");
}

static void console_task(void *argument)
{
    (void)argument;
    serial_command_parser_t parser;
    serial_command_t command;
    serial_command_parser_init(&parser);
    setvbuf(stdin, NULL, _IONBF, 0);
    log_help();

    for (;;) {
        const int byte = getchar();
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
            lidar_c1_log_status();
            break;
        case SERIAL_COMMAND_HELP: log_help(); break;
        case SERIAL_COMMAND_LIDAR_STATUS: lidar_c1_log_status(); break;
        case SERIAL_COMMAND_LIDAR_INFO: err = lidar_c1_request_info(); break;
        case SERIAL_COMMAND_LIDAR_HEALTH: err = lidar_c1_request_health(); break;
        case SERIAL_COMMAND_LIDAR_RX_TEST: err = lidar_c1_test_rx_line(); break;
        case SERIAL_COMMAND_ON: err = mesh_node_send_onoff(true, true); break;
        case SERIAL_COMMAND_OFF: err = mesh_node_send_onoff(false, true); break;
        case SERIAL_COMMAND_ON_UNACK: err = mesh_node_send_onoff(true, false); break;
        case SERIAL_COMMAND_OFF_UNACK: err = mesh_node_send_onoff(false, false); break;
        case SERIAL_COMMAND_TX_LOW: err = mesh_node_set_low_tx_power(true); break;
        case SERIAL_COMMAND_TX_NORMAL: err = mesh_node_set_low_tx_power(false); break;
        case SERIAL_COMMAND_EMPTY: break;
        default:
            ESP_LOGW("LAYER8_RIDAR_CONSOLE", "Command rejected/disabled: %s",
                     serial_command_name(command));
            break;
        }
        if (err != ESP_OK) {
            ESP_LOGW("LAYER8_RIDAR_CONSOLE", "Command %s failed: %s",
                     serial_command_name(command), esp_err_to_name(err));
        }
    }
}

void app_main(void)
{
    ESP_LOGI("LAYER8_RIDAR",
             "[LAYER8-RIDAR] BOOT_START project=nostos_esp32_layer8_ridar");
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE("LAYER8_RIDAR",
                 "NVS init failed (%s). STOP: existing settings preserved; no automatic erase.",
                 esp_err_to_name(err));
        return;
    }

    err = bridge_runtime_init();
    if (err != ESP_OK) {
        ESP_LOGE("LAYER8_RIDAR", "STM32 bridge init failed: %s", esp_err_to_name(err));
        return;
    }
    err = lidar_c1_init();
    if (err != ESP_OK) {
        ESP_LOGE("LAYER8_RIDAR", "C1 UART init failed: %s", esp_err_to_name(err));
        return;
    }
    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE("LAYER8_RIDAR", "Bluetooth init failed: %s", esp_err_to_name(err));
        return;
    }
    err = mesh_node_init();
    if (err != ESP_OK) {
        ESP_LOGE("LAYER8_RIDAR", "Mesh init failed: %s", esp_err_to_name(err));
        return;
    }
    if (xTaskCreate(console_task, "layer8_ridar_console", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE("LAYER8_RIDAR", "Console task allocation failed");
        return;
    }

    ESP_LOGI("LAYER8_RIDAR",
             "[LAYER8-RIDAR] APP_STARTED; UART1<->Mesh preserved; "
             "UART2 C1 diagnostics ready; scan disabled");
}
