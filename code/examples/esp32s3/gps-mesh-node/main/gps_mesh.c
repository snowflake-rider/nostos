#include "gps_mesh.h"
#include "gps_receiver.h"
#include <inttypes.h>
#include <string.h>
#include "esp_ble_mesh_networking_api.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define TAG "GPS_MESH"
#define GPS_OPCODE ESP_BLE_MESH_MODEL_OP_3(0x30, 0x02E5)
static esp_ble_mesh_model_op_t gps_ops[] = {
    ESP_BLE_MESH_MODEL_OP(GPS_OPCODE, 0), ESP_BLE_MESH_MODEL_OP_END,
};
/* Espressif example namespace: lab use only, not an assigned product CID. */
esp_ble_mesh_model_t gps_models[1] = {
    ESP_BLE_MESH_VENDOR_MODEL(0x02E5, 0x1001, gps_ops, NULL, NULL),
};
typedef enum { EVENT_RX, EVENT_SOURCE, EVENT_STATUS } event_kind_t;
typedef struct {
    event_kind_t kind;
    uint16_t source, destination;
    uint64_t received_ms;
    uint8_t bytes[GPS_PACKET_SIZE];
} gps_event_t;
static QueueHandle_t events;
static nvs_handle_t settings;
static gps_receiver_t receiver;

static uint64_t monotonic_ms(void) { return (uint64_t)esp_timer_get_time() / 1000U; }

static void model_callback(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param) {
    if (event != ESP_BLE_MESH_MODEL_OPERATION_EVT || !param ||
        param->model_operation.opcode != GPS_OPCODE) return;
    if (param->model_operation.length != GPS_PACKET_SIZE || !param->model_operation.msg ||
        !param->model_operation.ctx) {
        ESP_LOGW(TAG, "[GPS_REJECT] reason=invalid_length"); return;
    }
    gps_event_t item = {
        .kind = EVENT_RX, .source = param->model_operation.ctx->addr,
        .destination = param->model_operation.ctx->recv_dst, .received_ms = monotonic_ms(),
    };
    memcpy(item.bytes, param->model_operation.msg, sizeof(item.bytes));
    /* Mesh callback에서 포인터를 보관하지 않고 고정 크기 값을 복사한다. */
    if (xQueueSend(events, &item, 0) != pdPASS) ESP_LOGW(TAG, "[GPS_REJECT] reason=queue_full");
}

static void gps_task(void *unused) {
    (void)unused;
    for (;;) {
        gps_event_t event;
        if (xQueueReceive(events, &event, pdMS_TO_TICKS(100)) == pdPASS) {
            if (event.kind == EVENT_SOURCE) {
                if (receiver.source != event.source) {
                    esp_err_t err = nvs_set_u16(settings, "source", event.source);
                    if (err == ESP_OK) err = nvs_commit(settings);
                    if (err != ESP_OK) { ESP_LOGE(TAG, "[GPS_CONFIG_ERROR] err=%s", esp_err_to_name(err)); continue; }
                    gps_receiver_set_source(&receiver, event.source);
                }
                ESP_LOGI(TAG, "[GPS_SOURCE_SET] src=0x%04x", receiver.source);
            } else if (event.kind == EVENT_STATUS) {
                ESP_LOGI(TAG, "[GPS_STATUS] source=0x%04x has_sample=%d stale=%d cid=0x02E5 model=0x1001",
                    receiver.source, receiver.has_sample, receiver.stale);
            } else {
                gps_rx_result_t result = gps_receiver_accept(&receiver, event.source,
                    event.bytes, sizeof(event.bytes), event.received_ms);
                if (result == GPS_RX_ACCEPTED) {
                    const gps_packet_t *p = &receiver.latest;
                    ESP_LOGI(TAG, "[GPS_RX] src=0x%04x dst=0x%04x session=0x%08" PRIx32
                        " seq=%" PRIu32 " mode=%s measured_at=%" PRIu32
                        " lat=%.7f lon=%.7f accuracy_m=%.1f",
                        event.source, event.destination, p->session_id, p->sequence,
                        p->flags ? "TEST" : "LIVE", p->measured_at,
                        p->latitude_e7 / 1e7, p->longitude_e7 / 1e7, p->accuracy_dm / 10.0);
                } else {
                    const char *reason = result == GPS_RX_SOURCE_UNSET ? "source_unset" :
                        result == GPS_RX_OTHER_SOURCE ? "other_source" : result == GPS_RX_OLD ? "old_or_duplicate" : "invalid_fields";
                    ESP_LOGW(TAG, "[GPS_REJECT] src=0x%04x reason=%s", event.source, reason);
                }
            }
        }
        uint64_t now = monotonic_ms();
        if (gps_receiver_poll_stale(&receiver, now))
            ESP_LOGW(TAG, "[GPS_STALE] src=0x%04x session=0x%08" PRIx32 " last_seq=%" PRIu32 " elapsed_ms=%" PRIu64,
                receiver.source, receiver.latest.session_id, receiver.latest.sequence, now - receiver.received_ms);
    }
}

esp_err_t gps_mesh_init(void) {
    esp_err_t err = nvs_open("gpsdemo", NVS_READWRITE, &settings);
    if (err != ESP_OK) return err;
    uint16_t source = 0;
    err = nvs_get_u16(settings, "source", &source);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (source > 0x7fff) return ESP_ERR_INVALID_STATE;
    if (source) gps_receiver_set_source(&receiver, source);
    else ESP_LOGW(TAG, "[GPS_SOURCE_UNSET] configure: gps-source 0xNNNN");
    events = xQueueCreate(16, sizeof(gps_event_t));
    if (!events) return ESP_ERR_NO_MEM;
    err = esp_ble_mesh_register_custom_model_callback(model_callback);
    if (err != ESP_OK) return err;
    if (xTaskCreate(gps_task, "gps_rx", 4096, NULL, 4, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
esp_err_t gps_mesh_set_source(uint16_t source) {
    if (!source || source > 0x7fff) return ESP_ERR_INVALID_ARG;
    if (!events) return ESP_ERR_INVALID_STATE;
    gps_event_t event = {.kind = EVENT_SOURCE, .source = source};
    return xQueueSend(events, &event, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
void gps_mesh_log_status(void) {
    gps_event_t event = {.kind = EVENT_STATUS};
    if (events && xQueueSend(events, &event, 0) != pdPASS) ESP_LOGW(TAG, "GPS_STATUS_QUEUE_FULL");
}
