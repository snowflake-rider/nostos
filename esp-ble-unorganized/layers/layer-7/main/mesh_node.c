#include "mesh_node.h"

#include <inttypes.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"

#define TAG "LAYER_7_MESH"

#define CID_ESP 0x02E5
#define METADATA_MAGIC 0x4C374D45U
#define METADATA_VERSION 1U
#define METADATA_NAMESPACE "layer7"
#define METADATA_KEY "mesh_meta"

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint16_t version;
  uint16_t net_idx;
  uint16_t app_idx;
  uint16_t publication_addr;
  uint8_t tid;
  uint8_t onoff;
} mesh_metadata_t;

static uint8_t device_uuid[16] = {0x7A, 0x11, 0x07};
static char device_name[14];
static nvs_handle_t metadata_handle;
static bool metadata_open;
static bool factory_reset_requested;
static esp_power_level_t normal_adv_tx_power = ESP_PWR_LVL_INVALID;
static bool low_tx_power_enabled;

static mesh_metadata_t metadata = {
    .magic = METADATA_MAGIC,
    .version = METADATA_VERSION,
    .net_idx = ESP_BLE_MESH_KEY_UNUSED,
    .app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .publication_addr = ESP_BLE_MESH_ADDR_UNASSIGNED,
    .tid = 0,
    .onoff = 0,
};

static esp_ble_mesh_client_t onoff_client;

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
    .default_ttl = 7,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_server_pub, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    },
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_client_pub, 2 + 1, ROLE_NODE);

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_server_pub, &onoff_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_client_pub, &onoff_client),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

static esp_ble_mesh_prov_t provision = {
    .uuid = device_uuid,
    .output_size = 0,
    .output_actions = 0,
};

static const char *onoff_text(uint8_t value) { return value ? "ON" : "OFF"; }

static const char *relay_text(uint8_t value) {
  switch (value) {
  case ESP_BLE_MESH_RELAY_ENABLED:
    return "enabled";
  case ESP_BLE_MESH_RELAY_DISABLED:
    return "disabled";
  default:
    return "not-supported";
  }
}

static int tx_power_dbm(esp_power_level_t level) {
  static const int8_t dbm_by_level[] = {
      -24, -21, -18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18, 20,
  };

  if ((unsigned)level >= ARRAY_SIZE(dbm_by_level)) {
    return 127;
  }
  return dbm_by_level[level];
}

static void metadata_set_defaults(void) {
  memset(&metadata, 0, sizeof(metadata));
  metadata.magic = METADATA_MAGIC;
  metadata.version = METADATA_VERSION;
  metadata.net_idx = ESP_BLE_MESH_KEY_UNUSED;
  metadata.app_idx = ESP_BLE_MESH_KEY_UNUSED;
  metadata.publication_addr = ESP_BLE_MESH_ADDR_UNASSIGNED;
}

static esp_err_t metadata_store(void) {
  esp_err_t err;

  if (!metadata_open) {
    return ESP_ERR_INVALID_STATE;
  }
  err = nvs_set_blob(metadata_handle, METADATA_KEY, &metadata, sizeof(metadata));
  if (err == ESP_OK) {
    err = nvs_commit(metadata_handle);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] METADATA_STORE_FAILED err=%s",
             esp_err_to_name(err));
  }
  return err;
}

static void metadata_restore(void) {
  mesh_metadata_t restored;
  size_t length = sizeof(restored);
  esp_err_t err;

  if (!metadata_open) {
    return;
  }
  err = nvs_get_blob(metadata_handle, METADATA_KEY, &restored, &length);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "[LAYER-7] METADATA_RESTORE state=empty");
    return;
  }
  if (err != ESP_OK || length != sizeof(restored) ||
      restored.magic != METADATA_MAGIC || restored.version != METADATA_VERSION) {
    ESP_LOGW(TAG, "[LAYER-7] METADATA_RESTORE state=invalid err=%s length=%u",
             esp_err_to_name(err), (unsigned)length);
    metadata_set_defaults();
    return;
  }
  metadata = restored;
  onoff_server.state.onoff = metadata.onoff;
  ESP_LOGI(TAG,
           "[LAYER-7] METADATA_RESTORE state=ok net_idx=0x%04x app_idx=0x%04x pub=0x%04x tid=%u onoff=%s",
           metadata.net_idx, metadata.app_idx, metadata.publication_addr,
           metadata.tid, onoff_text(metadata.onoff));
}

static void metadata_erase(void) {
  if (metadata_open) {
    esp_err_t err = nvs_erase_key(metadata_handle, METADATA_KEY);
    if (err == ESP_OK) {
      err = nvs_commit(metadata_handle);
    }
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGE(TAG, "[LAYER-7] METADATA_ERASE_FAILED err=%s",
               esp_err_to_name(err));
    }
  }
  metadata_set_defaults();
}

static void build_identity(void) {
  uint8_t mac[6];
  esp_err_t err = esp_read_mac(mac, ESP_MAC_BT);

  if (err != ESP_OK) {
    const uint8_t *bt_mac = esp_bt_dev_get_address();
    if (bt_mac != NULL) {
      memcpy(mac, bt_mac, sizeof(mac));
    } else {
      ESP_ERROR_CHECK(err);
    }
  }

  memcpy(&device_uuid[10], mac, sizeof(mac));
  snprintf(device_name, sizeof(device_name), "ESP32-MESH-%02X", mac[5]);
  ESP_LOGI(TAG,
           "[LAYER-7] NODE_IDENTITY node=%02X name=%s uuid="
           "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
           mac[5], device_name, device_uuid[0], device_uuid[1], device_uuid[2],
           device_uuid[3], device_uuid[4], device_uuid[5], device_uuid[6],
           device_uuid[7], device_uuid[8], device_uuid[9], device_uuid[10],
           device_uuid[11], device_uuid[12], device_uuid[13], device_uuid[14],
           device_uuid[15]);
}

static bool client_appkey_is_bound(uint16_t app_idx) {
  if (app_idx == ESP_BLE_MESH_KEY_UNUSED || onoff_client.model == NULL) {
    return false;
  }
  for (size_t index = 0; index < ARRAY_SIZE(onoff_client.model->keys); index++) {
    if (onoff_client.model->keys[index] == app_idx) {
      return true;
    }
  }
  return false;
}

static uint16_t client_publication_address(void) {
  if (onoff_client.model == NULL || onoff_client.model->pub == NULL) {
    return ESP_BLE_MESH_ADDR_UNASSIGNED;
  }
  return onoff_client.model->pub->publish_addr;
}

static uint16_t server_subscription_address(void) {
  esp_ble_mesh_model_t *server_model = &root_models[1];

  for (size_t index = 0; index < ARRAY_SIZE(server_model->groups); index++) {
    if (server_model->groups[index] != ESP_BLE_MESH_ADDR_UNASSIGNED) {
      return server_model->groups[index];
    }
  }
  return ESP_BLE_MESH_ADDR_UNASSIGNED;
}

static void provisioning_callback(esp_ble_mesh_prov_cb_event_t event,
                                  esp_ble_mesh_prov_cb_param_t *param) {
  switch (event) {
  case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
    ESP_LOGI(TAG, "[LAYER-7] PROV_REGISTER err=%d",
             param->prov_register_comp.err_code);
    metadata_restore();
    break;
  case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
    ESP_LOGI(TAG, "[LAYER-7] UNPROVISIONED_READY bearers=PB-GATT|PB-ADV err=%d",
             param->node_prov_enable_comp.err_code);
    break;
  case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
    ESP_LOGI(TAG, "[LAYER-7] PROVISIONING_LINK_OPEN bearer=%s",
             param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV
                 ? "PB-ADV"
                 : "PB-GATT");
    break;
  case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
    ESP_LOGI(TAG, "[LAYER-7] PROVISIONING_LINK_CLOSE bearer=%s",
             param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV
                 ? "PB-ADV"
                 : "PB-GATT");
    break;
  case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
    metadata.net_idx = param->node_prov_complete.net_idx;
    ESP_LOGI(TAG,
             "[LAYER-7] PROVISIONING_COMPLETE net_idx=0x%04x primary_addr=0x%04x flags=0x%02x iv_index=0x%08" PRIx32,
             param->node_prov_complete.net_idx,
             param->node_prov_complete.addr,
             param->node_prov_complete.flags,
             param->node_prov_complete.iv_index);
    break;
  case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
    ESP_LOGI(TAG, "[LAYER-7] PROVISIONING_RESET_COMPLETE");
    metadata_erase();
    if (factory_reset_requested) {
      ESP_LOGI(TAG, "[LAYER-7] FACTORY_RESET_RESTARTING");
      vTaskDelay(pdMS_TO_TICKS(500));
      esp_restart();
    }
    break;
  case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
    ESP_LOGI(TAG, "[LAYER-7] DEVICE_NAME_SET name=%s err=%d", device_name,
             param->node_set_unprov_dev_name_comp.err_code);
    break;
  default:
    break;
  }
}

static void send_onoff_status(esp_ble_mesh_model_t *model,
                              esp_ble_mesh_msg_ctx_t *context) {
  esp_err_t err = esp_ble_mesh_server_model_send_msg(
      model, context, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
      sizeof(onoff_server.state.onoff), &onoff_server.state.onoff);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] ONOFF_STATUS_SEND_FAILED err=%s",
             esp_err_to_name(err));
  }
}

static void generic_server_callback(
    esp_ble_mesh_generic_server_cb_event_t event,
    esp_ble_mesh_generic_server_cb_param_t *param) {
  ESP_LOGI(TAG,
           "[LAYER-7] SERVER_EVENT event=%u opcode=0x%04" PRIx32
           " src=0x%04x dst=0x%04x ttl=%u",
           event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst,
           param->ctx.recv_ttl);

  if (event == ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT &&
      param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
    send_onoff_status(param->model, &param->ctx);
    return;
  }

  if (event == ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT &&
      (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
       param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK)) {
    metadata.onoff = param->value.set.onoff.onoff;
    onoff_server.state.onoff = metadata.onoff;
    (void)metadata_store();
    ESP_LOGI(TAG,
             "[LAYER-7] ONOFF_RX src=0x%04x dst=0x%04x recv_ttl=%u state=%s tid=%u",
             param->ctx.addr, param->ctx.recv_dst, param->ctx.recv_ttl,
             onoff_text(metadata.onoff), param->value.set.onoff.tid);

    if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
      send_onoff_status(param->model, &param->ctx);
    }
  }
}

static void generic_client_callback(
    esp_ble_mesh_generic_client_cb_event_t event,
    esp_ble_mesh_generic_client_cb_param_t *param) {
  uint32_t opcode = param->params != NULL ? param->params->opcode : 0;
  uint16_t destination =
      param->params != NULL ? param->params->ctx.addr : ESP_BLE_MESH_ADDR_UNASSIGNED;

  switch (event) {
  case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
    ESP_LOGI(TAG,
             "[LAYER-7] ONOFF_STATUS_RX src=0x%04x dst=0x%04x opcode=0x%04" PRIx32
             " err=%d state=%s",
             destination,
             param->params != NULL ? param->params->ctx.recv_dst
                                   : ESP_BLE_MESH_ADDR_UNASSIGNED,
             opcode, param->error_code,
             onoff_text(param->status_cb.onoff_status.present_onoff));
    break;
  case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
    ESP_LOGI(TAG,
             "[LAYER-7] CLIENT_PUBLISH_RX opcode=0x%04" PRIx32
             " src=0x%04x state=%s",
             opcode, destination,
             onoff_text(param->status_cb.onoff_status.present_onoff));
    break;
  case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
    ESP_LOGW(TAG,
             "[LAYER-7] ONOFF_TX_TIMEOUT opcode=0x%04" PRIx32 " dst=0x%04x",
             opcode, destination);
    break;
  default:
    ESP_LOGI(TAG,
             "[LAYER-7] CLIENT_EVENT event=%u opcode=0x%04" PRIx32
             " dst=0x%04x err=%d",
             event, opcode, destination, param->error_code);
    break;
  }
}

static void config_server_callback(esp_ble_mesh_cfg_server_cb_event_t event,
                                   esp_ble_mesh_cfg_server_cb_param_t *param) {
  if (event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
    return;
  }

  switch (param->ctx.recv_op) {
  case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
    metadata.net_idx = param->value.state_change.appkey_add.net_idx;
    ESP_LOGI(TAG, "[LAYER-7] APPKEY_ADDED net_idx=0x%04x app_idx=0x%04x",
             metadata.net_idx, param->value.state_change.appkey_add.app_idx);
    break;
  case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND: {
    const esp_ble_mesh_state_change_cfg_model_app_bind_t *bind =
        &param->value.state_change.mod_app_bind;
    ESP_LOGI(TAG,
             "[LAYER-7] MODEL_APP_BOUND element=0x%04x model=%s app_idx=0x%04x cid=0x%04x model_id=0x%04x",
             bind->element_addr,
             bind->model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV
                 ? "GEN_ONOFF_SERVER"
                 : bind->model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI
                       ? "GEN_ONOFF_CLIENT"
                       : "OTHER",
             bind->app_idx, bind->company_id, bind->model_id);
    if (bind->company_id == ESP_BLE_MESH_CID_NVAL &&
        bind->model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI) {
      metadata.app_idx = bind->app_idx;
      (void)metadata_store();
      ESP_LOGI(TAG, "[LAYER-7] CLIENT_APPKEY_READY app_idx=0x%04x",
               metadata.app_idx);
    }
    break;
  }
  case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET: {
    const esp_ble_mesh_state_change_cfg_mod_pub_set_t *publication =
        &param->value.state_change.mod_pub_set;
    ESP_LOGI(TAG,
             "[LAYER-7] MODEL_PUBLICATION_SET element=0x%04x pub=0x%04x app_idx=0x%04x cid=0x%04x model=0x%04x",
             publication->element_addr, publication->pub_addr,
             publication->app_idx, publication->company_id,
             publication->model_id);
    if (publication->company_id == ESP_BLE_MESH_CID_NVAL &&
        publication->model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI) {
      metadata.publication_addr = publication->pub_addr;
      metadata.app_idx = publication->app_idx;
      (void)metadata_store();
      ESP_LOGI(TAG,
               "[LAYER-7] CLIENT_PUBLICATION_READY address=0x%04x app_idx=0x%04x ttl=%u",
               metadata.publication_addr, metadata.app_idx,
               publication->pub_ttl);
    }
    break;
  }
  case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD: {
    const esp_ble_mesh_state_change_cfg_model_sub_add_t *subscription =
        &param->value.state_change.mod_sub_add;
    ESP_LOGI(TAG,
             "[LAYER-7] MODEL_SUBSCRIPTION_ADDED element=0x%04x group=0x%04x cid=0x%04x model=0x%04x",
             subscription->element_addr, subscription->sub_addr,
             subscription->company_id, subscription->model_id);
    if (subscription->company_id == ESP_BLE_MESH_CID_NVAL &&
        subscription->model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV) {
      ESP_LOGI(TAG,
               "[LAYER-7] GROUP_SUBSCRIBED model=GEN_ONOFF_SERVER address=0x%04x",
               subscription->sub_addr);
    }
    break;
  }
  case ESP_BLE_MESH_MODEL_OP_RELAY_SET:
    ESP_LOGI(TAG, "[LAYER-7] RELAY_STATE_CHANGED state=%s retransmit=0x%02x",
             relay_text(config_server.relay), config_server.relay_retransmit);
    break;
  default:
    break;
  }
}

esp_err_t mesh_node_init(void) {
  esp_err_t err;

  normal_adv_tx_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
  if (normal_adv_tx_power == ESP_PWR_LVL_INVALID) {
    normal_adv_tx_power = (esp_power_level_t)CONFIG_BT_CTRL_DFT_TX_POWER_LEVEL_EFF;
    ESP_LOGW(TAG,
             "[LAYER-7] TX_POWER_CAPTURE state=fallback normal_dbm=%d",
             tx_power_dbm(normal_adv_tx_power));
  } else {
    ESP_LOGI(TAG, "[LAYER-7] TX_POWER_CAPTURE state=ok normal_dbm=%d",
             tx_power_dbm(normal_adv_tx_power));
  }

  err = nvs_open(METADATA_NAMESPACE, NVS_READWRITE, &metadata_handle);
  if (err != ESP_OK) {
    return err;
  }
  metadata_open = true;
  build_identity();

  esp_ble_mesh_register_prov_callback(provisioning_callback);
  esp_ble_mesh_register_config_server_callback(config_server_callback);
  esp_ble_mesh_register_generic_server_callback(generic_server_callback);
  esp_ble_mesh_register_generic_client_callback(generic_client_callback);

  err = esp_ble_mesh_init(&provision, &composition);
  if (err != ESP_OK) {
    return err;
  }

  ESP_LOGI(TAG, "[LAYER-7] MESH_INITIALIZED");

  err = esp_ble_mesh_set_unprovisioned_device_name(device_name);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_ble_mesh_node_prov_enable(
      (esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV |
                                   ESP_BLE_MESH_PROV_GATT));
  if (err != ESP_OK) {
    return err;
  }

  ESP_LOGI(TAG,
           "[LAYER-7] MESH_READY models=config-server,generic-onoff-server,generic-onoff-client relay-default=disabled proxy=enabled");
  return ESP_OK;
}

esp_err_t mesh_node_set_low_tx_power(bool enabled) {
  esp_power_level_t requested =
      enabled ? ESP_PWR_LVL_N24 : normal_adv_tx_power;

  if (requested == ESP_PWR_LVL_INVALID) {
    ESP_LOGE(TAG, "[LAYER-7] TX_POWER_SET_FAILED reason=normal-not-captured");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, requested);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] TX_POWER_SET_FAILED mode=%s err=%s",
             enabled ? "low" : "normal", esp_err_to_name(err));
    return err;
  }

  esp_power_level_t applied = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
  if (applied == ESP_PWR_LVL_INVALID) {
    ESP_LOGE(TAG,
             "[LAYER-7] TX_POWER_SET_FAILED mode=%s reason=read-back-invalid",
             enabled ? "low" : "normal");
    return ESP_FAIL;
  }

  low_tx_power_enabled = enabled;
  ESP_LOGI(TAG,
           "[LAYER-7] TX_POWER_SET mode=%s requested_dbm=%d applied_dbm=%d volatile=yes",
           enabled ? "low" : "normal", tx_power_dbm(requested),
           tx_power_dbm(applied));
  return ESP_OK;
}

esp_err_t mesh_node_send_onoff(bool onoff, bool acknowledged) {
  esp_ble_mesh_generic_client_set_state_t set = {0};
  esp_ble_mesh_client_common_param_t common = {0};
  esp_err_t err;

  if (!esp_ble_mesh_node_is_provisioned()) {
    ESP_LOGE(TAG, "[LAYER-7] TX_REJECTED reason=not-provisioned");
    return ESP_ERR_INVALID_STATE;
  }
  if (metadata.net_idx == ESP_BLE_MESH_KEY_UNUSED ||
      metadata.app_idx == ESP_BLE_MESH_KEY_UNUSED ||
      !client_appkey_is_bound(metadata.app_idx)) {
    ESP_LOGE(TAG,
             "[LAYER-7] TX_REJECTED reason=appkey-not-ready net_idx=0x%04x app_idx=0x%04x",
             metadata.net_idx, metadata.app_idx);
    return ESP_ERR_INVALID_STATE;
  }
  uint16_t restored_publication = client_publication_address();
  if (restored_publication == ESP_BLE_MESH_ADDR_UNASSIGNED) {
    ESP_LOGE(TAG,
             "[LAYER-7] TX_REJECTED reason=client-publication-not-configured expected-group=0xC000");
    return ESP_ERR_INVALID_STATE;
  }
  if (metadata.publication_addr != restored_publication) {
    ESP_LOGW(TAG,
             "[LAYER-7] METADATA_RECONCILED field=client-publication stored=0x%04x restored=0x%04x",
             metadata.publication_addr, restored_publication);
    metadata.publication_addr = restored_publication;
    (void)metadata_store();
  }

  common.opcode = acknowledged ? ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET
                               : ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
  common.model = onoff_client.model;
  common.ctx.net_idx = metadata.net_idx;
  common.ctx.app_idx = metadata.app_idx;
  common.ctx.addr = metadata.publication_addr;
  common.ctx.send_ttl = 7;
  common.msg_timeout = 0;

  set.onoff_set.op_en = false;
  set.onoff_set.onoff = onoff ? 1U : 0U;
  set.onoff_set.tid = metadata.tid++;

  ESP_LOGI(TAG,
           "[LAYER-7] ONOFF_TX_REQUEST src=0x%04x dst=0x%04x state=%s tid=%u acknowledged=%s",
           esp_ble_mesh_get_primary_element_address(), common.ctx.addr,
           onoff_text(set.onoff_set.onoff), set.onoff_set.tid,
           acknowledged ? "yes" : "no");

  err = esp_ble_mesh_generic_client_set_state(&common, &set);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[LAYER-7] TX_ONOFF_FAILED err=%s", esp_err_to_name(err));
    return err;
  }

  (void)metadata_store();
  ESP_LOGI(TAG,
           "[LAYER-7] ONOFF_TX_ACCEPTED dst=0x%04x ttl=%u state=%s acknowledged=%s tid=%u",
           common.ctx.addr, common.ctx.send_ttl,
           onoff_text(set.onoff_set.onoff), acknowledged ? "yes" : "no",
           set.onoff_set.tid);
  return ESP_OK;
}

void mesh_node_log_status(void) {
  uint16_t publication = client_publication_address();
  uint16_t subscription = server_subscription_address();
  esp_power_level_t tx_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);

  ESP_LOGI(TAG,
           "[LAYER-7] STATUS name=%s provisioned=%s primary=0x%04x net_idx=0x%04x app_idx=0x%04x appkey_bound=%s client_pub=0x%04x server_sub=0x%04x relay=%s server_state=%s tid=%u tx_mode=%s tx_power_dbm=%d",
           device_name, esp_ble_mesh_node_is_provisioned() ? "yes" : "no",
           esp_ble_mesh_get_primary_element_address(), metadata.net_idx,
           metadata.app_idx,
           client_appkey_is_bound(metadata.app_idx) ? "yes" : "no",
           publication, subscription, relay_text(config_server.relay),
           onoff_text(onoff_server.state.onoff), metadata.tid,
           low_tx_power_enabled ? "low" : "normal", tx_power_dbm(tx_power));
}

esp_err_t mesh_node_factory_reset(void) {
  if (!esp_ble_mesh_node_is_provisioned()) {
    metadata_erase();
    ESP_LOGI(TAG, "[LAYER-7] FACTORY_RESET state=already-unprovisioned restarting");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
  }

  factory_reset_requested = true;
  ESP_LOGW(TAG, "[LAYER-7] FACTORY_RESET requested");
  esp_err_t err = esp_ble_mesh_node_local_reset();
  if (err != ESP_OK) {
    factory_reset_requested = false;
  }
  return err;
}
