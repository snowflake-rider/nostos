#include <stdio.h>

#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_mesh_example_init.h"
#include "mesh_node.h"
#include "serial_command.h"

#define TAG "LAYER_7"

static void execute_command(serial_command_t command) {
  esp_err_t err = ESP_OK;

  ESP_LOGI(TAG, "[LAYER-7] COMMAND name=%s", serial_command_name(command));

  switch (command) {
  case SERIAL_COMMAND_ON:
    err = mesh_node_send_onoff(true, true);
    break;
  case SERIAL_COMMAND_OFF:
    err = mesh_node_send_onoff(false, true);
    break;
  case SERIAL_COMMAND_ON_UNACK:
    err = mesh_node_send_onoff(true, false);
    break;
  case SERIAL_COMMAND_OFF_UNACK:
    err = mesh_node_send_onoff(false, false);
    break;
  case SERIAL_COMMAND_TX_LOW:
    err = mesh_node_set_low_tx_power(true);
    break;
  case SERIAL_COMMAND_TX_NORMAL:
    err = mesh_node_set_low_tx_power(false);
    break;
  case SERIAL_COMMAND_STATUS:
    mesh_node_log_status();
    return;
  case SERIAL_COMMAND_FACTORY_RESET:
    err = mesh_node_factory_reset();
    break;
  case SERIAL_COMMAND_EMPTY:
    return;
  case SERIAL_COMMAND_OVERFLOW:
    ESP_LOGE(TAG, "[LAYER-7] COMMAND_REJECTED reason=line-too-long");
    return;
  case SERIAL_COMMAND_UNKNOWN:
  case SERIAL_COMMAND_NONE:
  default:
    ESP_LOGE(TAG, "[LAYER-7] COMMAND_REJECTED reason=unknown-command");
    return;
  }

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] COMMAND_FAILED name=%s err=%s",
             serial_command_name(command), esp_err_to_name(err));
  }
}

static void serial_command_task(void *argument) {
  serial_command_parser_t parser;
  serial_command_t command;

  (void)argument;
  serial_command_parser_init(&parser);
  setvbuf(stdin, NULL, _IONBF, 0);

  ESP_LOGI(TAG,
           "[LAYER-7] SERIAL_READY commands=on,off,on-unack,off-unack,tx-low,tx-normal,status,factory-reset");

  while (true) {
    int byte = getchar();
    if (byte == EOF) {
      clearerr(stdin);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (serial_command_parser_feed(&parser, (uint8_t)byte, &command)) {
      execute_command(command);
    }
  }
}

void app_main(void) {
  esp_err_t err;

  ESP_LOGI(TAG,
           "[LAYER-7] BOOT_SUCCESS target=esp32s3 idf=v%d.%d.%d",
           ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR,
           ESP_IDF_VERSION_PATCH);

  err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  err = bluetooth_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] BLUETOOTH_INIT_FAILED err=%s",
             esp_err_to_name(err));
    return;
  }

  err = mesh_node_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] MESH_INIT_FAILED err=%s", esp_err_to_name(err));
    return;
  }

  if (xTaskCreate(serial_command_task, "layer7_serial", 4096, NULL, 5, NULL) !=
      pdPASS) {
    ESP_LOGE(TAG, "[LAYER-7] SERIAL_TASK_CREATE_FAILED");
    return;
  }

  ESP_LOGI(TAG, "[LAYER-7] NODE_ACTIVE");
  mesh_node_log_status();
}
