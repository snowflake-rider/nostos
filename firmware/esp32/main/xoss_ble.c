#include "xoss_ble.h"

#include "bridge_runtime.h"
#include "mesh_node.h"
#include "speed_sensor.h"

#include "esp_bt_defs.h"
#include "esp_ble_mesh_ble_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <limits.h>
#include <string.h>

#define TAG "XOSS_CSC"
#define XOSS_APP_ID 0U
#define XOSS_NOTIFICATION_MAX 11U
#define XOSS_QUEUE_LENGTH 8U
#define XOSS_RECONNECT_MS 2000U

typedef enum {
    XOSS_EVENT_NOTIFICATION,
    XOSS_EVENT_DISCONNECTED
} xoss_event_kind_t;

typedef struct {
    xoss_event_kind_t kind;
    uint8_t length;
    uint8_t bytes[XOSS_NOTIFICATION_MAX];
} xoss_event_t;

static StaticQueue_t event_queue_control;
static uint8_t event_queue_storage[XOSS_QUEUE_LENGTH * sizeof(xoss_event_t)];
static QueueHandle_t event_queue;
static TaskHandle_t xoss_task_handle;
static esp_gatt_if_t client_if = ESP_GATT_IF_NONE;
static uint16_t connection_id;
static uint16_t service_start_handle;
static uint16_t service_end_handle;
static uint16_t measurement_handle;
static esp_bd_addr_t remote_address;
static bool connecting;
static bool connected;
static bool service_found;
static bool subscribed;
static speed_sensor_state_t speed_state;
static portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t notifications_received;
static uint32_t notifications_dropped;
static uint32_t rides_forwarded;
static uint32_t rides_rejected;
static uint32_t last_ride_ms;
static bool initialized;
static bool scan_enabled;

static esp_bt_uuid_t service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = SPEED_SENSOR_CSC_SERVICE_UUID},
};
static const esp_bt_uuid_t measurement_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = SPEED_SENSOR_CSC_MEASUREMENT_UUID},
};
static const esp_bt_uuid_t cccd_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG},
};
static uint32_t now_ms(void)
{
    return (uint32_t)((uint64_t)esp_timer_get_time() / 1000U);
}

static bool enqueue_event(const xoss_event_t *event)
{
    if (event_queue == NULL || xQueueSend(event_queue, event, 0U) != pdPASS) {
        portENTER_CRITICAL(&status_lock);
        ++notifications_dropped;
        portEXIT_CRITICAL(&status_lock);
        return false;
    }
    return true;
}

static void close_connection_after_setup_error(void)
{
    esp_err_t result = esp_ble_gattc_close(client_if, connection_id);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "DISCONNECT_START_FAILED %s", esp_err_to_name(result));
    }
}

static bool advertised_name_matches(uint8_t *data, uint16_t length)
{
    uint8_t name_length = 0U;
    uint8_t *name = esp_ble_resolve_adv_data_by_type(
        data,
        length,
        ESP_BLE_AD_TYPE_NAME_CMPL,
        &name_length);
    if (name == NULL) {
        name = esp_ble_resolve_adv_data_by_type(
            data,
            length,
            ESP_BLE_AD_TYPE_NAME_SHORT,
            &name_length);
    }
    size_t configured_length = strlen(CONFIG_NOSTOS_XOSS_DEVICE_NAME);
    return name != NULL && configured_length == (size_t)name_length &&
        memcmp(name, CONFIG_NOSTOS_XOSS_DEVICE_NAME, configured_length) == 0;
}

static bool advertised_csc_service(uint8_t *data, uint16_t length)
{
    const esp_ble_adv_data_type types[] = {
        ESP_BLE_AD_TYPE_16SRV_CMPL,
        ESP_BLE_AD_TYPE_16SRV_PART,
    };
    for (size_t type = 0U; type < sizeof(types) / sizeof(types[0]); ++type) {
        uint8_t uuid_length = 0U;
        uint8_t *uuids = esp_ble_resolve_adv_data_by_type(
            data, length, types[type], &uuid_length);
        for (uint8_t offset = 0U; uuids != NULL && offset + 1U < uuid_length;
             offset = (uint8_t)(offset + 2U)) {
            uint16_t uuid = (uint16_t)uuids[offset] |
                (uint16_t)((uint16_t)uuids[offset + 1U] << 8U);
            if (uuid == SPEED_SENSOR_CSC_SERVICE_UUID) return true;
        }
    }
    return false;
}

static esp_err_t start_mesh_coexistence_scan(void)
{
    if (mesh_node_primary() != CONFIG_NOSTOS_SOURCE1_ADDRESS) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_ble_mesh_ble_scan_param_t scan = {.duration = 0U};
    return esp_ble_mesh_start_ble_scanning(&scan);
}

static void mesh_ble_callback(
    esp_ble_mesh_ble_cb_event_t event,
    esp_ble_mesh_ble_cb_param_t *parameter)
{
    if (event == ESP_BLE_MESH_START_BLE_SCANNING_COMP_EVT) {
        bool ready = parameter != NULL && parameter->start_ble_scan_comp.err_code == 0;
        portENTER_CRITICAL(&status_lock);
        scan_enabled = ready;
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGI(TAG, "SCAN_READY ready=%u err=%d", ready,
            parameter != NULL ? parameter->start_ble_scan_comp.err_code : -1);
        return;
    }
    if (event == ESP_BLE_MESH_STOP_BLE_SCANNING_COMP_EVT) {
        if (parameter != NULL && parameter->stop_ble_scan_comp.err_code == 0) {
            portENTER_CRITICAL(&status_lock);
            scan_enabled = false;
            portEXIT_CRITICAL(&status_lock);
        }
        return;
    }
    if (event != ESP_BLE_MESH_SCAN_BLE_ADVERTISING_PKT_EVT ||
        parameter == NULL || parameter->scan_ble_adv_pkt.data == NULL ||
        mesh_node_primary() != CONFIG_NOSTOS_SOURCE1_ADDRESS ||
        connecting || connected) {
        return;
    }

    esp_ble_mesh_ble_adv_rpt_t *report = &parameter->scan_ble_adv_pkt;
    bool name_match = advertised_name_matches(report->data, report->length);
    bool service_match = advertised_csc_service(report->data, report->length);
    if (!name_match && !service_match) return;

    connecting = true;
    memcpy(remote_address, report->addr, sizeof(remote_address));
    (void)esp_ble_mesh_stop_ble_scanning();
    ESP_LOGI(TAG, "DISCOVERED name=%u csc=%u rssi=%d address=" ESP_BD_ADDR_STR,
        name_match, service_match, report->rssi, ESP_BD_ADDR_HEX(remote_address));
    esp_err_t result = esp_ble_gattc_open(
        client_if, remote_address, report->addr_type, true);
    if (result != ESP_OK) {
        connecting = false;
        ESP_LOGE(TAG, "CONNECT_START_FAILED %s", esp_err_to_name(result));
        xoss_event_t disconnected = {.kind = XOSS_EVENT_DISCONNECTED};
        (void)enqueue_event(&disconnected);
    }
}

static void register_measurement_notification(void)
{
    esp_gattc_char_elem_t characteristic;
    uint16_t count = 1U;
    esp_gatt_status_t status = esp_ble_gattc_get_char_by_uuid(
        client_if,
        connection_id,
        service_start_handle,
        service_end_handle,
        measurement_uuid,
        &characteristic,
        &count);
    if (status != ESP_GATT_OK || count == 0U ||
        (characteristic.properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY) == 0U) {
        ESP_LOGE(TAG, "CSC_MEASUREMENT_NOT_FOUND status=%d count=%u",
            status, (unsigned)count);
        close_connection_after_setup_error();
        return;
    }
    measurement_handle = characteristic.char_handle;
    esp_err_t result = esp_ble_gattc_register_for_notify(
        client_if, remote_address, measurement_handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "NOTIFY_REGISTER_START_FAILED %s", esp_err_to_name(result));
        close_connection_after_setup_error();
    }
}

static void enable_measurement_notification(uint16_t handle)
{
    esp_gattc_descr_elem_t descriptor;
    uint16_t count = 1U;
    esp_gatt_status_t status = esp_ble_gattc_get_descr_by_char_handle(
        client_if, connection_id, handle, cccd_uuid, &descriptor, &count);
    if (status != ESP_GATT_OK || count == 0U) {
        ESP_LOGE(TAG, "CCCD_NOT_FOUND status=%d count=%u", status, (unsigned)count);
        close_connection_after_setup_error();
        return;
    }
    uint8_t enable[2] = {0x01U, 0x00U};
    esp_err_t result = esp_ble_gattc_write_char_descr(
        client_if,
        connection_id,
        descriptor.handle,
        sizeof(enable),
        enable,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "CCCD_WRITE_START_FAILED %s", esp_err_to_name(result));
        close_connection_after_setup_error();
    }
}

static void gattc_callback(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t *parameter)
{
    if (event == ESP_GATTC_REG_EVT) {
        if (parameter->reg.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "GATTC_REGISTER_FAILED status=%d", parameter->reg.status);
            return;
        }
        client_if = gattc_if;
        esp_err_t result = start_mesh_coexistence_scan();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "SCAN_START_FAILED %s", esp_err_to_name(result));
        }
        return;
    }
    if (gattc_if != ESP_GATT_IF_NONE && gattc_if != client_if) return;

    switch (event) {
    case ESP_GATTC_CONNECT_EVT:
        connecting = false;
        connected = true;
        connection_id = parameter->connect.conn_id;
        memcpy(remote_address, parameter->connect.remote_bda, sizeof(remote_address));
        (void)esp_ble_gattc_send_mtu_req(client_if, connection_id);
        ESP_LOGI(TAG, "CONNECTED address=" ESP_BD_ADDR_STR,
            ESP_BD_ADDR_HEX(remote_address));
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (parameter->dis_srvc_cmpl.status == ESP_GATT_OK) {
            (void)esp_ble_gattc_search_service(client_if, connection_id,
                &service_uuid);
        } else {
            ESP_LOGE(TAG, "SERVICE_DISCOVERY_FAILED status=%d",
                parameter->dis_srvc_cmpl.status);
            close_connection_after_setup_error();
        }
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        if (parameter->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 &&
            parameter->search_res.srvc_id.uuid.uuid.uuid16 ==
                SPEED_SENSOR_CSC_SERVICE_UUID) {
            service_found = true;
            service_start_handle = parameter->search_res.start_handle;
            service_end_handle = parameter->search_res.end_handle;
        }
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (parameter->search_cmpl.status == ESP_GATT_OK && service_found) {
            register_measurement_notification();
        } else {
            ESP_LOGE(TAG, "CSC_SERVICE_NOT_FOUND status=%d",
                parameter->search_cmpl.status);
            close_connection_after_setup_error();
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        if (parameter->reg_for_notify.status == ESP_GATT_OK) {
            enable_measurement_notification(parameter->reg_for_notify.handle);
        } else {
            ESP_LOGE(TAG, "NOTIFY_REGISTER_FAILED status=%d",
                parameter->reg_for_notify.status);
            close_connection_after_setup_error();
        }
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (parameter->write.status == ESP_GATT_OK) {
            subscribed = true;
            ESP_LOGI(TAG, "CSC_NOTIFY_READY circumference_mm=%d",
                CONFIG_NOSTOS_XOSS_WHEEL_CIRCUMFERENCE_MM);
        } else {
            ESP_LOGE(TAG, "CCCD_WRITE_FAILED status=%d", parameter->write.status);
            close_connection_after_setup_error();
        }
        break;
    case ESP_GATTC_NOTIFY_EVT:
        if (parameter->notify.handle == measurement_handle &&
            parameter->notify.value_len <= XOSS_NOTIFICATION_MAX) {
            xoss_event_t notification = {
                .kind = XOSS_EVENT_NOTIFICATION,
                .length = (uint8_t)parameter->notify.value_len,
            };
            memcpy(notification.bytes, parameter->notify.value,
                parameter->notify.value_len);
            if (enqueue_event(&notification)) {
                portENTER_CRITICAL(&status_lock);
                ++notifications_received;
                portEXIT_CRITICAL(&status_lock);
            }
        } else {
            portENTER_CRITICAL(&status_lock);
            ++notifications_dropped;
            portEXIT_CRITICAL(&status_lock);
        }
        break;
    case ESP_GATTC_OPEN_EVT:
        if (parameter->open.status != ESP_GATT_OK) {
            connecting = false;
            xoss_event_t disconnected = {.kind = XOSS_EVENT_DISCONNECTED};
            (void)enqueue_event(&disconnected);
        }
        break;
    case ESP_GATTC_DISCONNECT_EVT: {
        connecting = false;
        connected = false;
        subscribed = false;
        service_found = false;
        measurement_handle = 0U;
        xoss_event_t disconnected = {.kind = XOSS_EVENT_DISCONNECTED};
        (void)enqueue_event(&disconnected);
        break;
    }
    default:
        break;
    }
}

static void process_notification(const xoss_event_t *event)
{
    speed_sensor_measurement_t measurement;
    speed_sensor_sample_t sample;
    speed_sensor_result_t result = speed_sensor_decode_csc(
        event->bytes, event->length, &measurement);
    if (result == SPEED_SENSOR_OK) {
        result = speed_sensor_update(&speed_state, &measurement,
            CONFIG_NOSTOS_XOSS_WHEEL_CIRCUMFERENCE_MM, &sample);
    }
    bool distance_in_range = result == SPEED_SENSOR_OK && sample.valid &&
        sample.distance_mm <= UINT32_MAX;
    esp_err_t forward_result = distance_in_range
        ? bridge_runtime_send_sensor_ride(
            true, sample.kmh_x10, (uint32_t)sample.distance_mm)
        : ESP_ERR_INVALID_ARG;
    if (distance_in_range && forward_result == ESP_OK) {
        portENTER_CRITICAL(&status_lock);
        ++rides_forwarded;
        last_ride_ms = now_ms();
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGI(TAG, "RIDE kmh_x10=%u distance_mm=%" PRIu32,
            (unsigned)sample.kmh_x10, (uint32_t)sample.distance_mm);
    } else if (result == SPEED_SENSOR_OK && sample.valid && !distance_in_range) {
        portENTER_CRITICAL(&status_lock);
        ++rides_rejected;
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGW(TAG, "RIDE_DROP distance_mm=%" PRIu64 " exceeds uint32",
            sample.distance_mm);
    } else if (distance_in_range) {
        portENTER_CRITICAL(&status_lock);
        ++rides_rejected;
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGW(TAG, "RIDE_FORWARD_FAILED %s", esp_err_to_name(forward_result));
    } else if (result != SPEED_SENSOR_BASELINE && result != SPEED_SENSOR_NO_UPDATE) {
        portENTER_CRITICAL(&status_lock);
        ++rides_rejected;
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGW(TAG, "CSC_DROP %s", speed_sensor_result_name(result));
    }
}

static void xoss_task(void *argument)
{
    (void)argument;
    bool stale_reported = false;
    for (;;) {
        xoss_event_t event;
        if (xQueueReceive(event_queue, &event, pdMS_TO_TICKS(500U)) == pdTRUE) {
            if (event.kind == XOSS_EVENT_NOTIFICATION) {
                process_notification(&event);
                stale_reported = false;
            } else if (event.kind == XOSS_EVENT_DISCONNECTED) {
                speed_sensor_reset(&speed_state);
                (void)bridge_runtime_send_sensor_ride(false, 0U, 0U);
                stale_reported = true;
                vTaskDelay(pdMS_TO_TICKS(XOSS_RECONNECT_MS));
                if (mesh_node_primary() == CONFIG_NOSTOS_SOURCE1_ADDRESS) {
                    esp_err_t result = start_mesh_coexistence_scan();
                    if (result != ESP_OK) {
                        ESP_LOGE(TAG, "SCAN_RESTART_FAILED %s",
                            esp_err_to_name(result));
                    }
                }
            }
        }

        portENTER_CRITICAL(&status_lock);
        uint32_t last = last_ride_ms;
        portEXIT_CRITICAL(&status_lock);
        if (!stale_reported && last != 0U &&
            (uint32_t)(now_ms() - last) >= CONFIG_NOSTOS_XOSS_STALE_MS) {
            (void)bridge_runtime_send_sensor_ride(false, 0U, 0U);
            stale_reported = true;
            ESP_LOGW(TAG, "RIDE_STALE");
        }
    }
}

esp_err_t xoss_ble_init(void)
{
    if (mesh_node_primary() != CONFIG_NOSTOS_SOURCE1_ADDRESS) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (CONFIG_NOSTOS_XOSS_WHEEL_CIRCUMFERENCE_MM <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    event_queue = xQueueCreateStatic(
        XOSS_QUEUE_LENGTH,
        sizeof(xoss_event_t),
        event_queue_storage,
        &event_queue_control);
    if (event_queue == NULL) return ESP_ERR_NO_MEM;
    speed_sensor_reset(&speed_state);

    if (xTaskCreate(xoss_task, "xoss_csc", 4096U, NULL, 4U,
        &xoss_task_handle) != pdPASS) {
        event_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = esp_ble_mesh_register_ble_callback(mesh_ble_callback);
    if (result == ESP_OK) result = esp_ble_gattc_register_callback(gattc_callback);
    if (result == ESP_OK) result = esp_ble_gattc_app_register(XOSS_APP_ID);
    if (result != ESP_OK) {
        vTaskDelete(xoss_task_handle);
        xoss_task_handle = NULL;
        event_queue = NULL;
    } else {
        portENTER_CRITICAL(&status_lock);
        initialized = true;
        portEXIT_CRITICAL(&status_lock);
    }
    return result;
}

void xoss_ble_log_status(void)
{
    portENTER_CRITICAL(&status_lock);
    uint32_t received = notifications_received;
    uint32_t dropped = notifications_dropped;
    uint32_t forwarded = rides_forwarded;
    uint32_t rejected = rides_rejected;
    uint32_t last = last_ride_ms;
    bool enabled = initialized;
    bool scanning = scan_enabled;
    portEXIT_CRITICAL(&status_lock);
    ESP_LOGI(TAG, "STATUS enabled=%u scan_ready=%u connected=%u subscribed=%u notifications=%" PRIu32
        " dropped=%" PRIu32 " rides_forwarded=%" PRIu32 " rides_rejected=%" PRIu32
        " last_ride_ms=%" PRIu32,
        enabled, scanning, connected, subscribed, received, dropped, forwarded, rejected, last);
}
