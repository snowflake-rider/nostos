/* Adapted from the workspace's Layer 7 composition and Generic OnOff flow.
 * Vendor model API reference: ESP-IDF v5.5.5 vendor_server example.
 * No sensor interpretation or application-level mesh republishing here. */
#include "mesh_node.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "bridge_runtime.h"
#include "event_protocol.h"
#include "nostos_protocol.h"

#define TAG "LAYER_8_MESH"
#if CONFIG_NOSTOS_PROTOCOL_V2
#define EVENT_OPCODE ESP_BLE_MESH_MODEL_OP_3(0x21, BSG_COMPANY_ID)
#else
#define EVENT_OPCODE ESP_BLE_MESH_MODEL_OP_3(0x20, BSG_COMPANY_ID)
#endif
#define KEY_MAP_MAGIC UINT32_C(0x42534701)

typedef struct { uint16_t app, net; } key_pair_t;
typedef struct {
    uint32_t magic;
    key_pair_t pairs[CONFIG_BLE_MESH_APP_KEY_COUNT];
} key_map_t;
/* Only key INDEX metadata, never key material. Owned by serialized Mesh callbacks. */
static key_map_t key_map;
static nvs_handle_t metadata_nvs;
typedef struct {
    bool ready, onoff_ready, subscribed;
    uint16_t primary, app, net, onoff_app, onoff_net, pub;
    uint8_t period, retransmit, ttl, relay, onoff_state;
} mesh_snapshot_t;
static mesh_snapshot_t snapshot;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_power_level_t normal_power;
static uint8_t tid; /* console task only; debug OnOff state is volatile */
static uint8_t uuid[16] = {0x7A, 0x11, 0x08};
static char device_name[20];
static esp_ble_mesh_client_t onoff_client;

static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(1, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
    .default_ttl = 7,
};
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_server_pub, 5, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_client_pub, 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
                 .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP},
};
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_server_pub, &onoff_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_client_pub, &onoff_client),
};
/* Configuration exists, but this buffer is NEVER filled/published. Explicit send
 * copies each payload, avoiding shared publication-buffer overwrite/replay. */
ESP_BLE_MESH_MODEL_PUB_DEFINE(event_pub, 5, ROLE_NODE);
static esp_ble_mesh_model_op_t event_ops[] = {
    ESP_BLE_MESH_MODEL_OP(EVENT_OPCODE, 0), /* codec counts short/malformed payloads too */
    ESP_BLE_MESH_MODEL_OP_END,
};
static esp_ble_mesh_model_t vendor_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(BSG_COMPANY_ID, BSG_MODEL_ID, event_ops, &event_pub, NULL),
};
static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vendor_models),
};
static esp_ble_mesh_comp_t composition = {
    .cid = BSG_COMPANY_ID, .element_count = ARRAY_SIZE(elements), .elements = elements,
};
static esp_ble_mesh_prov_t provision = {.uuid = uuid};

static mesh_snapshot_t read_snapshot(void)
{
    portENTER_CRITICAL(&state_lock);
    mesh_snapshot_t copy = snapshot;
    portEXIT_CRITICAL(&state_lock);
    return copy;
}

static void clear_key_map(void)
{
    key_map.magic = KEY_MAP_MAGIC;
    for (size_t i = 0; i < ARRAY_SIZE(key_map.pairs); ++i) {
        key_map.pairs[i] = (key_pair_t){ESP_BLE_MESH_KEY_UNUSED, ESP_BLE_MESH_KEY_UNUSED};
    }
}

static void save_key_map(void)
{
    esp_err_t err = nvs_set_blob(metadata_nvs, "key_indices", &key_map, sizeof(key_map));
    if (err == ESP_OK) err = nvs_commit(metadata_nvs);
    if (err != ESP_OK) ESP_LOGE(TAG, "INDEX_SAVE_FAILED err=%s; reboot may require AppKey Add again", esp_err_to_name(err));
}

static uint16_t net_for_app(uint16_t app)
{
    for (size_t i = 0; i < ARRAY_SIZE(key_map.pairs); ++i) {
        key_pair_t p = key_map.pairs[i];
        if (p.app == app && p.app <= 0xFFF && p.net <= 0xFFF &&
            esp_ble_mesh_node_get_local_app_key(p.app) != NULL &&
            esp_ble_mesh_node_get_local_net_key(p.net) != NULL) return p.net;
    }
    return ESP_BLE_MESH_KEY_UNUSED;
}

static bool bound_to(const esp_ble_mesh_model_t *model, uint16_t app)
{
    if (app > 0xFFF) return false;
    for (size_t i = 0; i < ARRAY_SIZE(model->keys); ++i) {
        if (model->keys[i] == app) return true;
    }
    return false;
}

/* Read stack-owned model configuration only from Mesh callback context, then
 * publish a small locked snapshot for application tasks. No cached key bytes. */
static void refresh_snapshot(void)
{
    mesh_snapshot_t s = {0};
    s.primary = esp_ble_mesh_get_primary_element_address();
    s.app = event_pub.app_idx;
    s.net = net_for_app(s.app);
    s.pub = event_pub.publish_addr;
    s.period = event_pub.period;
    s.retransmit = event_pub.retransmit;
    s.ttl = event_pub.ttl;
    s.relay = config_server.relay;
    s.onoff_state = onoff_server.state.onoff;
    s.subscribed = esp_ble_mesh_is_model_subscribed_to_group(&vendor_models[0], BSG_EVENT_GROUP) != NULL;
    s.ready = s.primary != 0 && s.net != ESP_BLE_MESH_KEY_UNUSED &&
              bound_to(&vendor_models[0], s.app) && s.pub == BSG_EVENT_GROUP &&
              s.period == 0 && s.retransmit == 0 && s.ttl == 7;
    s.onoff_app = onoff_client_pub.app_idx;
    s.onoff_net = net_for_app(s.onoff_app);
    s.onoff_ready = s.primary != 0 && s.onoff_net != ESP_BLE_MESH_KEY_UNUSED &&
                    bound_to(&root_models[2], s.onoff_app) && onoff_client_pub.publish_addr == 0xC000;
    portENTER_CRITICAL(&state_lock);
    snapshot = s;
    portEXIT_CRITICAL(&state_lock);
}

static void provisioning_callback(esp_ble_mesh_prov_cb_event_t event,
                                   esp_ble_mesh_prov_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_PROV_REGISTER_COMP_EVT) {
        if (param->prov_register_comp.err_code != 0) {
            ESP_LOGE(TAG, "REGISTER_FAILED err=%d", param->prov_register_comp.err_code);
            return;
        }
        refresh_snapshot();
        mesh_node_log_status();
    } else if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT) {
        refresh_snapshot();
        ESP_LOGI(TAG, "PROVISIONED primary=0x%04x; Vendor model configuration still required",
                 param->node_prov_complete.addr);
    } else if (event == ESP_BLE_MESH_NODE_PROV_RESET_EVT) {
        /* A remote provisioner may reset this node; never initiate reset ourselves. */
        clear_key_map();
        save_key_map();
        refresh_snapshot();
        ESP_LOGW(TAG, "REMOTE_NODE_RESET; no automatic reboot or erase");
    }
}

static void config_callback(esp_ble_mesh_cfg_server_cb_event_t event,
                            esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) return;
    /* Delete stale mappings after key deletion; avoid assigning a guessed NetKey. */
    for (size_t i = 0; i < ARRAY_SIZE(key_map.pairs); ++i) {
        if (net_for_app(key_map.pairs[i].app) == ESP_BLE_MESH_KEY_UNUSED) {
            key_map.pairs[i] = (key_pair_t){ESP_BLE_MESH_KEY_UNUSED, ESP_BLE_MESH_KEY_UNUSED};
        }
    }
    if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD) {
        const esp_ble_mesh_state_change_cfg_appkey_add_t *add = &param->value.state_change.appkey_add;
        size_t slot = ARRAY_SIZE(key_map.pairs);
        for (size_t i = 0; i < ARRAY_SIZE(key_map.pairs); ++i) {
            if (key_map.pairs[i].app == add->app_idx) {
                slot = i;
                break;
            }
            if (slot == ARRAY_SIZE(key_map.pairs) && key_map.pairs[i].app == ESP_BLE_MESH_KEY_UNUSED) slot = i;
        }
        if (slot < ARRAY_SIZE(key_map.pairs)) key_map.pairs[slot] = (key_pair_t){add->app_idx, add->net_idx};
    }
    /* Includes unbind/delete/publication changes, not just initial bind. */
    refresh_snapshot();
    save_key_map();
    ESP_LOGI(TAG, "CONFIG_CHANGED opcode=0x%04" PRIx32, param->ctx.recv_op);
    mesh_node_log_status();
}

static void custom_callback(esp_ble_mesh_model_cb_event_t event,
                            esp_ble_mesh_model_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_MODEL_OPERATION_EVT &&
        param->model_operation.model == &vendor_models[0] &&
        param->model_operation.opcode == EVENT_OPCODE && param->model_operation.ctx != NULL) {
        bridge_runtime_mesh_rx(param->model_operation.msg, param->model_operation.length,
                               param->model_operation.ctx->addr, mesh_node_primary());
    } else if (event == ESP_BLE_MESH_MODEL_SEND_COMP_EVT &&
               param->model_send_comp.model == &vendor_models[0] &&
               param->model_send_comp.opcode == EVENT_OPCODE) {
        bridge_runtime_mesh_complete(param->model_send_comp.err_code);
    }
    /* PUBLISH_UPDATE intentionally ignored: no periodic event or replay. */
}

static void generic_server_callback(esp_ble_mesh_generic_server_cb_event_t event,
                                    esp_ble_mesh_generic_server_cb_param_t *param)
{
    bool get = event == ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT &&
               param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET;
    bool set = event == ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT &&
               (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
                param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK);
    if (set) {
        onoff_server.state.onoff = param->value.set.onoff.onoff;
        refresh_snapshot();
        ESP_LOGI(TAG, "ONOFF_RX src=0x%04x value=%u", param->ctx.addr, onoff_server.state.onoff);
    }
    if (get || (set && param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET)) {
        esp_err_t err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, 1, &onoff_server.state.onoff);
        if (err != ESP_OK) ESP_LOGW(TAG, "ONOFF_STATUS_FAILED %s", esp_err_to_name(err));
    }
}

static void generic_client_callback(esp_ble_mesh_generic_client_cb_event_t event,
                                    esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "ONOFF_CLIENT event=%u err=%d src=0x%04x", event, param->error_code,
             param->params != NULL ? param->params->ctx.addr : 0);
}

esp_err_t mesh_node_init(void)
{
    clear_key_map();
    esp_err_t err = nvs_open("bsg_bridge", NVS_READWRITE, &metadata_nvs);
    if (err != ESP_OK) return err;
    key_map_t stored;
    size_t size = sizeof(stored);
    err = nvs_get_blob(metadata_nvs, "key_indices", &stored, &size);
    if (err == ESP_OK && size == sizeof(stored) && stored.magic == KEY_MAP_MAGIC) key_map = stored;
    else ESP_LOGW(TAG, "NO_KEY_INDEX_MAP; configure/re-add AppKey before event TX");

    uint8_t mac[6];
    err = esp_read_mac(mac, ESP_MAC_BT);
    if (err != ESP_OK) return err;
    memcpy(&uuid[10], mac, sizeof(mac));
    snprintf(device_name, sizeof(device_name), "ESP32-L8-%02X%02X", mac[4], mac[5]);
    normal_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    if (normal_power == ESP_PWR_LVL_INVALID) normal_power = CONFIG_BT_CTRL_DFT_TX_POWER_LEVEL_EFF;

    if ((err = esp_ble_mesh_register_prov_callback(provisioning_callback)) != ESP_OK) return err;
    if ((err = esp_ble_mesh_register_config_server_callback(config_callback)) != ESP_OK) return err;
    if ((err = esp_ble_mesh_register_custom_model_callback(custom_callback)) != ESP_OK) return err;
    if ((err = esp_ble_mesh_register_generic_server_callback(generic_server_callback)) != ESP_OK) return err;
    if ((err = esp_ble_mesh_register_generic_client_callback(generic_client_callback)) != ESP_OK) return err;
    if ((err = esp_ble_mesh_init(&provision, &composition)) != ESP_OK) return err;
    if ((err = esp_ble_mesh_set_unprovisioned_device_name(device_name)) != ESP_OK) return err;
    if (!esp_ble_mesh_node_is_provisioned()) {
        err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    }
    return err;
}

bool mesh_node_ready(void) { return read_snapshot().ready; }
uint16_t mesh_node_primary(void) { return read_snapshot().primary; }

esp_err_t mesh_node_send_event(const uint8_t *wire, size_t length)
{
#if CONFIG_NOSTOS_PROTOCOL_V2
    nostos_message_t decoded;
    nostos_result_t validation=nostos_message_decode(wire,length,&decoded);
    if(validation!=NOSTOS_OK && validation!=NOSTOS_UNSUPPORTED_TYPE) return ESP_ERR_INVALID_ARG;
#else
    uint8_t id;
    if (!event_decode(wire, length, &id)) return ESP_ERR_INVALID_ARG;
#endif
    mesh_snapshot_t s = read_snapshot();
    if (!s.ready) return ESP_ERR_INVALID_STATE;
    esp_ble_mesh_msg_ctx_t ctx = {
        .net_idx = s.net, .app_idx = s.app, .addr = BSG_EVENT_GROUP, .send_ttl = 7,
    };
#if CONFIG_NOSTOS_PROTOCOL_V2
    /* Mesh SDK owns segmentation, TTL, encryption and reassembly. No UART CRC. */
    return esp_ble_mesh_server_model_send_msg(&vendor_models[0], &ctx, EVENT_OPCODE, (uint16_t)length, (uint8_t *)wire);
#else
    uint8_t copy[EVENT_WIRE_SIZE] = {EVENT_WIRE_VERSION, id};
    /* IDF v5.5.5 deep-copies ctx and payload before this API returns. */
    return esp_ble_mesh_server_model_send_msg(&vendor_models[0], &ctx, EVENT_OPCODE, sizeof(copy), copy);
#endif
}

esp_err_t mesh_node_send_onoff(bool onoff, bool acknowledged)
{
    mesh_snapshot_t s = read_snapshot();
    if (!s.onoff_ready) return ESP_ERR_INVALID_STATE;
    esp_ble_mesh_client_common_param_t common = {
        .opcode = acknowledged ? ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET : ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK,
        .model = &root_models[2],
        .ctx = {.net_idx = s.onoff_net, .app_idx = s.onoff_app, .addr = 0xC000, .send_ttl = 7},
    };
    esp_ble_mesh_generic_client_set_state_t set = {0};
    set.onoff_set.onoff = onoff;
    set.onoff_set.tid = tid++;
    return esp_ble_mesh_generic_client_set_state(&common, &set);
}

esp_err_t mesh_node_set_low_tx_power(bool enabled)
{
    return esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, enabled ? ESP_PWR_LVL_N24 : normal_power);
}

void mesh_node_log_status(void)
{
    mesh_snapshot_t s = read_snapshot();
    ESP_LOGI(TAG, "STATUS name=%s primary=0x%04x event_ready=%u net=0x%04x app=0x%04x"
             " pub=0x%04x sub_C001=%u ttl=%u period=%u retransmit=%u relay=%u onoff_ready=%u state=%u",
             device_name, s.primary, s.ready, s.net, s.app, s.pub, s.subscribed,
             s.ttl, s.period, s.retransmit, s.relay, s.onoff_ready, s.onoff_state);
}
