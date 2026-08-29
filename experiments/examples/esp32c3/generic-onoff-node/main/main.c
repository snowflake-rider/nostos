/*
 * ESP32-C3 ESP-BLE-MESH Generic OnOff Node
 *
 * Based on Espressif's ESP-BLE-MESH onoff_server example.
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"

#include "ble_mesh_example_init.h"

#define TAG "BIKE_MESH"
#define CID_ESP 0x02E5

/* ble_mesh_get_dev_uuid()가 칩 정보로 나머지 바이트를 채운다. */
static uint8_t device_uuid[16] = {0xDD, 0xDD};

/*
 * Relay 기능은 펌웨어에 포함하지만 부팅 시에는 끈다.
 * Provisioner 앱에서 필요한 노드만 Relay Enabled로 바꿀 수 있다.
 */
static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(1, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 2,
};

/* Generic OnOff Status 메시지를 발행할 버퍼와 Server 상태다. */
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_publication, 2 + 3, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

/* 한 Element 안에 Configuration Server와 Generic OnOff Server를 둔다. */
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_publication, &onoff_server),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

/* 첫 실습에서는 OOB 입력 없이 스마트폰 앱으로 Provisioning한다. */
static esp_ble_mesh_prov_t provisioning = {
    .uuid = device_uuid,
    .output_size = 0,
    .output_actions = 0,
};

static void provisioning_callback(esp_ble_mesh_prov_cb_event_t event,
                                  esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "mesh register: err=%d",
                 param->prov_register_comp.err_code);
        break;

    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "provisioning link opened: %s",
                 param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV
                     ? "PB-ADV"
                     : "PB-GATT");
        break;

    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "provisioning link closed");
        break;

    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG,
                 "provisioned: net_idx=0x%04x addr=0x%04x flags=0x%02x "
                 "iv_index=0x%08" PRIx32,
                 param->node_prov_complete.net_idx,
                 param->node_prov_complete.addr,
                 param->node_prov_complete.flags,
                 param->node_prov_complete.iv_index);
        break;

    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        ESP_LOGW(TAG, "node provisioning data reset");
        break;

    default:
        break;
    }
}

static void generic_server_callback(esp_ble_mesh_generic_server_cb_event_t event,
                                    esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGI(TAG,
             "model event=0x%02x opcode=0x%04" PRIx32
             " src=0x%04x dst=0x%04x",
             event,
             param->ctx.recv_op,
             param->ctx.addr,
             param->ctx.recv_dst);

    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT) {
        return;
    }

    if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
        param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
        uint8_t state = param->value.state_change.onoff_set.onoff;

        /* 다음 단계에서 이 위치를 실제 LED, 부저 또는 UART 출력으로 바꾼다. */
        ESP_LOGI(TAG, "ONOFF state changed: %s", state ? "ON" : "OFF");
    }
}

static esp_err_t mesh_init(void)
{
    esp_err_t err;

    esp_ble_mesh_register_prov_callback(provisioning_callback);
    esp_ble_mesh_register_generic_server_callback(generic_server_callback);

    err = esp_ble_mesh_init(&provisioning, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_mesh_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(
        (esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV |
                                    ESP_BLE_MESH_PROV_GATT));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "provisioning enable failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ready: unprovisioned BLE Mesh node");
    ESP_LOG_BUFFER_HEX("device_uuid", device_uuid, sizeof(device_uuid));
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth init failed: %s", esp_err_to_name(err));
        return;
    }

    ble_mesh_get_dev_uuid(device_uuid);
    ESP_ERROR_CHECK(mesh_init());
}
