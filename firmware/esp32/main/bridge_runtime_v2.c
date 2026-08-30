/* Explicit v2 bridge. One image derives its source from the provisioned Mesh
 * primary address; UART TX and durable identity writes stay in the worker. */
#include "bridge_runtime.h"
#include "mesh_node.h"
#include "nostos_bridge.h"
#include "sensor_link.h"
#if CONFIG_NOSTOS_XOSS_SPEED_SENSOR
#include "xoss_ble.h"
#endif
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include <inttypes.h>

#define TAG "NOSTOS_V2"
#define DATA_UART UART_NUM_1
#define IDENTITY_NVS_NAMESPACE "nostos_ident"
#define IDENTITY_NVS_KEY "session"
#define CONTROL_QUEUE_LENGTH 4U
#define IDENTITY_POLL_MS 100U
#define IDENTITY_RETRY_MS 1000U

typedef enum {
    UART_ROUTE_IDLE = 0,
    UART_ROUTE_NOSTOS,
    UART_ROUTE_SENSOR_LINK,
} uart_route_t;

typedef struct {
    bool bridge_initialized;
    bool identity_loaded;
    bool identity_confirmed;
    bool identity_waiting_ack;
    bool identity_session_from_hello;
    uint8_t local_source;
    uint16_t primary_address;
    uint32_t session_id;
} identity_state_t;

static nostos_bridge_t bridge;
static nostos_uart_parser_t nostos_parser;
static sensor_link_parser_t sensor_parser;
static uart_route_t uart_route;
static identity_state_t identity;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static QueueHandle_t events;
static StaticSemaphore_t reset_mutex_control;
static SemaphoreHandle_t reset_mutex;
static bool stm_boot_reset_in_progress;
static uint32_t stm_boot_epoch;

static StaticQueue_t ride_queue_control;
static uint8_t ride_queue_storage[sizeof(sensor_link_ride_t)];
static QueueHandle_t ride_queue;

static StaticQueue_t control_queue_control;
static uint8_t control_queue_storage[CONTROL_QUEUE_LENGTH * sizeof(sensor_link_message_t)];
static QueueHandle_t control_queue;

static uint32_t accepted, rejected, async_ok, async_failed;
static uint32_t ride_uart_ok, ride_uart_failed, ride_uart_deferred;
static uint32_t identity_uart_ok, identity_uart_failed;
static uint32_t approve_uart_ok, approve_uart_failed;
static uint32_t control_received, control_rejected, control_overflow;
/* Worker-owned retry schedule; identity fields themselves remain locked. */
static bool identity_advertised_once;
static uint32_t identity_last_tx_ms;

/* Verified provisioned primary-unicast map. It is deliberately identical in
 * every image; source is never selected by a per-board build option. */
static const nostos_peer_t peers[NOSTOS_NODE_COUNT] = {
    {CONFIG_NOSTOS_SOURCE1_ADDRESS, 1U, 1U}, /* 76 */
    {CONFIG_NOSTOS_SOURCE2_ADDRESS, 2U, 2U}, /* D6 */
    {CONFIG_NOSTOS_SOURCE3_ADDRESS, 3U, 3U}, /* B6 */
};

static uint32_t now_ms(void)
{
    return (uint32_t)((uint64_t)esp_timer_get_time() / 1000U);
}

static void counter_increment(uint32_t *counter)
{
    portENTER_CRITICAL(&lock);
    ++*counter;
    portEXIT_CRITICAL(&lock);
}

static void reset_epoch_counters_locked(void)
{
    accepted = 0U;
    rejected = 0U;
    /* Mesh completion callbacks have no transaction/epoch token and can arrive
     * after an STM boot boundary. Keep these two ESP-uptime totals monotonic so
     * a delayed old completion is never attributed to a freshly zeroed epoch. */
    ride_uart_ok = 0U;
    ride_uart_failed = 0U;
    ride_uart_deferred = 0U;
    identity_uart_ok = 0U;
    identity_uart_failed = 0U;
    approve_uart_ok = 0U;
    approve_uart_failed = 0U;
    control_received = 0U;
    control_rejected = 0U;
    control_overflow = 0U;
    identity_advertised_once = false;
    identity_last_tx_ms = 0U;
}

static esp_err_t reset_xoss_runtime_session(void)
{
#if CONFIG_NOSTOS_XOSS_SPEED_SENSOR
    return xoss_ble_reset_runtime_session();
#else
    return ESP_OK;
#endif
}

/* Worker-only. The gate makes Mesh RX and UART/XOSS producers reject work
 * while both FreeRTOS queues and the locked bridge are moved to one epoch. */
static bool begin_stm_boot_boundary(void)
{
    portENTER_CRITICAL(&lock);
    stm_boot_reset_in_progress = true;
    portEXIT_CRITICAL(&lock);

    if (reset_mutex == NULL || control_queue == NULL || ride_queue == NULL) {
        ESP_LOGE(TAG, "STM_BOOT_RESET_NOT_READY gate=closed");
        return false;
    }

    if (xSemaphoreTake(reset_mutex, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGE(TAG, "STM_BOOT_QUEUE_RESET_TIMEOUT");
        return false;
    }

    (void)xQueueReset(control_queue);
    (void)xQueueReset(ride_queue);

    portENTER_CRITICAL(&lock);
    uint8_t source = identity.local_source;
    nostos_result_t reset_result = identity.bridge_initialized
        ? nostos_bridge_init(&bridge, source, peers)
        : NOSTOS_NOT_READY;
    if (reset_result == NOSTOS_OK) {
        ++stm_boot_epoch;
        if (stm_boot_epoch == 0U) stm_boot_epoch = 1U;
        reset_epoch_counters_locked();
    }
    portEXIT_CRITICAL(&lock);
    xSemaphoreGive(reset_mutex);

    if (reset_result != NOSTOS_OK) {
        ESP_LOGE(TAG, "STM_BOOT_RESET_FAILED result=%s",
                 nostos_result_name(reset_result));
        return false;
    }

    esp_err_t xoss_result = reset_xoss_runtime_session();
    if (xoss_result != ESP_OK) {
        ESP_LOGE(TAG, "STM_BOOT_XOSS_RESET_FAILED err=%s",
                 esp_err_to_name(xoss_result));
        return false;
    }

    return true;
}

static void end_stm_boot_boundary(void)
{
    portENTER_CRITICAL(&lock);
    stm_boot_reset_in_progress = false;
    uint32_t epoch = stm_boot_epoch;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG, "STM_BOOT_RUNTIME_RESET epoch=%" PRIu32, epoch);
}

static uint16_t get16le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8));
}

static uint32_t get32le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint8_t source_for_address(uint16_t address)
{
    if (address == 0U) return 0U;
    for (size_t i = 0; i < NOSTOS_NODE_COUNT; ++i) {
        if (peers[i].mesh_address == address) return peers[i].source_id;
    }
    return 0U;
}

static bool bridge_ready(void)
{
    uint8_t source;
    uint16_t primary;
    bool initialized;
    bool resetting;
    portENTER_CRITICAL(&lock);
    source = identity.local_source;
    primary = identity.primary_address;
    initialized = identity.bridge_initialized;
    resetting = stm_boot_reset_in_progress;
    portEXIT_CRITICAL(&lock);
    return !resetting && initialized && source != 0U &&
           source_for_address(primary) == source &&
           mesh_node_ready() && mesh_node_primary() == primary;
}

static bool uart_write(const uint8_t *bytes, size_t length)
{
    size_t sent = 0U;
    uint32_t start = now_ms();
    while (sent < length) {
        if ((uint32_t)(now_ms() - start) > 20U) return false;
        int count = uart_tx_chars(DATA_UART, (const char *)bytes + sent,
                                  (uint32_t)(length - sent));
        if (count < 0) return false;
        sent += (size_t)count;
        if (sent < length) vTaskDelay(1);
    }
    return uart_wait_tx_done(DATA_UART, pdMS_TO_TICKS(20)) == ESP_OK;
}

static bool uart_send_nostos(const uint8_t *wire, size_t length)
{
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    if (nostos_uart_encode(wire, length, frame, sizeof(frame), &frame_length) != NOSTOS_OK) {
        return false;
    }
    return uart_write(frame, frame_length);
    /* Failed/partial writes are not retried: the next frame flag resynchronizes. */
}

static esp_err_t nvs_load_session(uint32_t *session)
{
    if (session == NULL) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(IDENTITY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    uint32_t stored = 0U;
    err = nvs_get_u32(handle, IDENTITY_NVS_KEY, &stored);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    nvs_close(handle);
    if (err == ESP_OK) *session = stored;
    return err;
}

static esp_err_t nvs_commit_next_session(uint32_t current, uint32_t *next)
{
    if (next == NULL) return ESP_ERR_INVALID_ARG;
    if (current == UINT32_MAX) return ESP_ERR_INVALID_STATE;
    uint32_t candidate = current + 1U;
    if (candidate == 0U) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(IDENTITY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(handle, IDENTITY_NVS_KEY, candidate);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK) *next = candidate;
    return err;
}

/* Worker-only: bind bridge identity after Mesh exposes a verified primary. */
static bool refresh_identity_binding(void)
{
    uint16_t primary = mesh_node_primary();
    uint8_t source = source_for_address(primary);
    portENTER_CRITICAL(&lock);
    bool already_bound = identity.bridge_initialized && identity.identity_loaded &&
                         identity.primary_address == primary &&
                         identity.local_source == source && source != 0U;
    if (source == 0U && identity.bridge_initialized) {
        identity = (identity_state_t){0};
        identity_advertised_once = false;
    }
    portEXIT_CRITICAL(&lock);
    if (already_bound) return true;
    if (source == 0U) return false;

    uint32_t stored_session = 0U;
    esp_err_t err = nvs_load_session(&stored_session);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IDENTITY_LOAD_FAILED primary=0x%04x err=%s",
                 primary, esp_err_to_name(err));
        return false;
    }

    /* Recheck after the I/O: a provisioning reset may have changed identity. */
    if (mesh_node_primary() != primary) return false;
    portENTER_CRITICAL(&lock);
    nostos_result_t result = nostos_bridge_init(&bridge, source, peers);
    if (result == NOSTOS_OK) {
        identity = (identity_state_t){
            .bridge_initialized = true,
            .identity_loaded = true,
            .identity_confirmed = false,
            /* After an ESP-only reboot, first offer the durable identity so a
             * still-running STM can continue without resetting its sequence. */
            .identity_waiting_ack = stored_session != 0U,
            .identity_session_from_hello = false,
            .local_source = source,
            .primary_address = primary,
            .session_id = stored_session,
        };
    }
    portEXIT_CRITICAL(&lock);
    if (result != NOSTOS_OK) {
        ESP_LOGE(TAG, "IDENTITY_BIND_FAILED result=%s", nostos_result_name(result));
        return false;
    }
    identity_advertised_once = false;
    ESP_LOGI(TAG, "IDENTITY_BOUND primary=0x%04x source=%u durable_session=%" PRIu32,
             primary, source, stored_session);
    return true;
}

/* Worker-only: advance once per new handshake, commit before advertising it. */
static bool advance_identity_session(void)
{
    uint32_t current;
    uint8_t source;
    uint16_t primary;
    portENTER_CRITICAL(&lock);
    current = identity.session_id;
    source = identity.local_source;
    primary = identity.primary_address;
    bool bound = identity.bridge_initialized && identity.identity_loaded;
    portEXIT_CRITICAL(&lock);
    if (!bound || source == 0U || mesh_node_primary() != primary) return false;

    uint32_t next = 0U;
    esp_err_t err = nvs_commit_next_session(current, &next);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IDENTITY_ADVANCE_FAILED source=%u current=%" PRIu32 " err=%s",
                 source, current, esp_err_to_name(err));
        return false;
    }
    if (mesh_node_primary() != primary) return false;
    portENTER_CRITICAL(&lock);
    if (identity.bridge_initialized && identity.local_source == source &&
        identity.primary_address == primary) {
        identity.session_id = next;
        identity.identity_confirmed = false;
        identity.identity_waiting_ack = true;
        identity.identity_session_from_hello = true;
    } else {
        next = 0U;
    }
    portEXIT_CRITICAL(&lock);
    if (next == 0U) return false;
    identity_advertised_once = false;
    ESP_LOGI(TAG, "IDENTITY_SESSION_COMMITTED source=%u session=%" PRIu32, source, next);
    return true;
}

/* Worker-only. A rebooted ESP advertises its durable current identity without
 * waiting for STM HELLO, then retries at a bounded interval until confirmed. */
static bool advertise_current_identity(bool force)
{
    uint32_t now = now_ms();
    portENTER_CRITICAL(&lock);
    bool waiting = !stm_boot_reset_in_progress &&
                   identity.bridge_initialized && identity.identity_loaded &&
                   identity.identity_waiting_ack && identity.session_id != 0U;
    uint8_t source = identity.local_source;
    uint32_t session = identity.session_id;
    portEXIT_CRITICAL(&lock);
    if (!waiting) return false;
    if (!force && identity_advertised_once &&
        (uint32_t)(now - identity_last_tx_ms) < IDENTITY_RETRY_MS) return false;

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t result = sensor_link_encode_identity(source, session, frame, &length);
    bool sent = result == SENSOR_LINK_OK && uart_write(frame, length);
    identity_advertised_once = true;
    identity_last_tx_ms = now;
    counter_increment(sent ? &identity_uart_ok : &identity_uart_failed);
    ESP_LOGI(TAG, "IDENTITY_TX source=%u session=%" PRIu32 " api=%s",
             source, session, sent ? "accepted" : "failed");
    return true;
}

static bool process_local_control(void)
{
    sensor_link_message_t message;
    if (control_queue == NULL || xQueueReceive(control_queue, &message, 0) != pdTRUE) return false;

    if (message.type == SENSOR_LINK_HELLO) {
        if (!refresh_identity_binding()) {
            counter_increment(&identity_uart_failed);
            ESP_LOGW(TAG, "IDENTITY_DEFERRED no verified Mesh primary");
            return true;
        }
        portENTER_CRITICAL(&lock);
        /* A HELLO is STM boot evidence. Advance once for this boot, including
         * the both-sides-reboot case where an old durable session is pending.
         * Repeated HELLOs before ACK only resend the same committed session. */
        bool advance = identity.identity_confirmed ||
                       !identity.identity_session_from_hello;
        uint32_t session = identity.session_id;
        portEXIT_CRITICAL(&lock);
        bool new_stm_boot = advance || session == 0U;
        if (new_stm_boot) {
            if (!begin_stm_boot_boundary()) {
                counter_increment(&identity_uart_failed);
                return true;
            }
            if (!advance_identity_session()) {
                counter_increment(&identity_uart_failed);
                ESP_LOGE(TAG, "STM_BOOT_IDENTITY_ADVANCE_FAILED gate=closed");
                return true;
            }
            end_stm_boot_boundary();
        }
        (void)advertise_current_identity(true);
        return true;
    }

    if (message.type == SENSOR_LINK_IDENTITY_ACK) {
        bool matched;
        uint16_t primary = mesh_node_primary();
        portENTER_CRITICAL(&lock);
        matched = !stm_boot_reset_in_progress &&
                  identity.bridge_initialized && identity.session_id != 0U &&
                  identity.primary_address == primary &&
                  message.identity.source_id == identity.local_source &&
                  message.identity.session_id == identity.session_id;
        if (matched) {
            identity.identity_waiting_ack = false;
            identity.identity_confirmed = true;
        }
        portEXIT_CRITICAL(&lock);
        if (matched) {
            ESP_LOGI(TAG, "IDENTITY_ACK source=%u session=%" PRIu32,
                     message.identity.source_id, message.identity.session_id);
        } else {
            counter_increment(&control_rejected);
            ESP_LOGW(TAG, "IDENTITY_ACK_REJECT source=%u session=%" PRIu32,
                     message.identity.source_id, message.identity.session_id);
        }
        return true;
    }

    counter_increment(&control_rejected);
    ESP_LOGW(TAG, "LOCAL_CONTROL_REJECT type=0x%02x", message.type);
    return true;
}

static nostos_result_t enqueue_remote(const uint8_t *wire, size_t length,
                                      uint16_t mesh_source)
{
    bool ready = bridge_ready();
    uint32_t now = now_ms();
    portENTER_CRITICAL(&lock);
    nostos_result_t result = stm_boot_reset_in_progress
        ? NOSTOS_NOT_READY
        : identity.bridge_initialized
        ? nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length,
                               mesh_source, now, ready)
        : NOSTOS_NOT_READY;
    if (!stm_boot_reset_in_progress) {
        if (result == NOSTOS_OK) ++accepted; else ++rejected;
    }
    portEXIT_CRITICAL(&lock);
    if (result == NOSTOS_OK) xTaskNotifyGive(worker);
    return result;
}

/* UART RX task: validate against the exact identity atomically with enqueue. */
static nostos_result_t enqueue_local(const uint8_t *wire, size_t length)
{
    nostos_message_t decoded;
    nostos_result_t validation = nostos_message_decode(wire, length, &decoded);
    /* Preserve v2 forward compatibility while requiring complete validation
     * for every type this firmware understands. */
    if (validation != NOSTOS_OK && validation != NOSTOS_UNSUPPORTED_TYPE) return validation;
    bool ready = bridge_ready();
    uint32_t now = now_ms();
    uint8_t claimed_source = wire[2];
    uint32_t claimed_session = get32le(wire + 3U);
    bool confirmed_now = false;
    portENTER_CRITICAL(&lock);
    nostos_result_t result = NOSTOS_SESSION_REQUIRED;
    if (stm_boot_reset_in_progress) {
        result = NOSTOS_NOT_READY;
    } else if (identity.bridge_initialized && identity.identity_loaded &&
        identity.session_id != 0U && claimed_source == identity.local_source &&
        claimed_session == identity.session_id) {
        /* A valid official packet proves that the STM accepted this identity,
         * even when Mesh configuration currently prevents forwarding. */
        identity.identity_waiting_ack = false;
        confirmed_now = !identity.identity_confirmed;
        identity.identity_confirmed = true;
        result = nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                      0U, now, ready);
    } else if (identity.bridge_initialized && claimed_source != identity.local_source) {
        result = NOSTOS_UNAUTHORIZED;
    }
    if (!stm_boot_reset_in_progress) {
        if (result == NOSTOS_OK) ++accepted; else ++rejected;
    }
    portEXIT_CRITICAL(&lock);
    if (result == NOSTOS_OK || confirmed_now) xTaskNotifyGive(worker);
    return result;
}

static bool process_sensor_ride(void)
{
    sensor_link_ride_t ride;
    if (ride_queue == NULL || xQueuePeek(ride_queue, &ride, 0) != pdTRUE) return false;
    portENTER_CRITICAL(&lock);
    bool confirmed = identity.identity_confirmed;
    bool resetting = stm_boot_reset_in_progress;
    portEXIT_CRITICAL(&lock);
    if (!confirmed || resetting) {
        /* Keep only the latest atomic ride queued until v2 identity is confirmed. */
        return false;
    }
    (void)xQueueReceive(ride_queue, &ride, 0);
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t result = sensor_link_encode_ride(
        ride.valid, ride.kmh_x10, ride.distance_mm, frame, &length);
    bool sent = result == SENSOR_LINK_OK && uart_write(frame, length);
    counter_increment(sent ? &ride_uart_ok : &ride_uart_failed);
    ESP_LOGI(TAG, "RIDE_UART_TX valid=%u kmh_x10=%u distance_mm=%" PRIu32 " api=%s",
             ride.valid, (unsigned)ride.kmh_x10, ride.distance_mm,
             sent ? "accepted" : "failed");
    return true;
}

static bool send_remote_approval(const nostos_job_t *job)
{
    if (job == NULL || job->length < NOSTOS_HEADER_SIZE) return false;
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t result = sensor_link_encode_approve_session(
        job->wire[2], get32le(job->wire + 3U), get16le(job->wire + 7U),
        frame, &length);
    bool sent = result == SENSOR_LINK_OK && uart_write(frame, length);
    counter_increment(sent ? &approve_uart_ok : &approve_uart_failed);
    return sent;
}

static nostos_result_t process_one(void)
{
    nostos_job_t job;
    bool ready = bridge_ready();
    uint32_t now = now_ms();
    portENTER_CRITICAL(&lock);
    nostos_result_t result = stm_boot_reset_in_progress
        ? NOSTOS_EMPTY
        : identity.bridge_initialized
        ? nostos_bridge_next(&bridge, now, ready, &job)
        : NOSTOS_EMPTY;
    portEXIT_CRITICAL(&lock);
    if (result == NOSTOS_EMPTY) return result;
    if (result != NOSTOS_OK) {
        ESP_LOGW(TAG, "TX_JOB_DROP result=%s", nostos_result_name(result));
        return result;
    }

    bool sent;
    if (job.direction == NOSTOS_TO_MESH) {
        sent = mesh_node_send_event(job.wire, job.length) == ESP_OK;
    } else {
        /* The local trust grant and official packet are adjacent writes by the
         * sole UART owner. Never expose an unapproved remote session to STM. */
        sent = send_remote_approval(&job) && uart_send_nostos(job.wire, job.length);
    }
    ESP_LOGI(TAG, "%s type=0x%02x source=%u len=%u api=%s",
             job.direction == NOSTOS_TO_MESH ? "MESH_TX" : "UART_TX",
             job.wire[1], job.wire[2], (unsigned)job.length,
             sent ? "accepted" : "failed");
    return sent ? NOSTOS_OK : NOSTOS_IO_ERROR;
}

static void worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(IDENTITY_POLL_MS));
        (void)refresh_identity_binding();
        for (;;) {
            bool control_processed = process_local_control();
            bool identity_processed = advertise_current_identity(false);
            nostos_result_t result = process_one();
            bool ride_processed = process_sensor_ride();
            if (!control_processed && !identity_processed && result == NOSTOS_EMPTY &&
                !ride_processed) break;
            vTaskDelay(1);
        }
    }
}

static void queue_local_control(const sensor_link_message_t *message)
{
    if (message == NULL || (message->type != SENSOR_LINK_HELLO &&
                            message->type != SENSOR_LINK_IDENTITY_ACK)) {
        counter_increment(&control_rejected);
        return;
    }
    if (reset_mutex == NULL ||
        xSemaphoreTake(reset_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        ESP_LOGW(TAG, "LOCAL_CONTROL_QUEUE_BUSY type=0x%02x", message->type);
        return;
    }
    portENTER_CRITICAL(&lock);
    bool resetting = stm_boot_reset_in_progress;
    portEXIT_CRITICAL(&lock);
    bool allow_retry_hello = resetting && message->type == SENSOR_LINK_HELLO;
    BaseType_t queued = (resetting && !allow_retry_hello) ? pdFALSE
        : xQueueSend(control_queue, message, 0);
    if (resetting && !allow_retry_hello) {
        xSemaphoreGive(reset_mutex);
        return;
    }
    if (queued != pdTRUE) {
        counter_increment(&control_overflow);
        xSemaphoreGive(reset_mutex);
        ESP_LOGW(TAG, "LOCAL_CONTROL_QUEUE_FULL type=0x%02x", message->type);
        return;
    }
    counter_increment(&control_received);
    xSemaphoreGive(reset_mutex);
    xTaskNotifyGive(worker);
}

static nostos_result_t consume_uart_byte(uint8_t byte)
{
    uint32_t now = now_ms();
    if (uart_route == UART_ROUTE_IDLE) {
        if (byte == NOSTOS_UART_FLAG) {
            uint8_t ignored[NOSTOS_WIRE_MAX];
            size_t ignored_length = 0U;
            uart_route = UART_ROUTE_NOSTOS;
            return nostos_uart_feed(&nostos_parser, byte, now, ignored, &ignored_length);
        }
        if (byte == SENSOR_LINK_PREAMBLE_0) {
            sensor_link_message_t ignored = {0};
            uart_route = UART_ROUTE_SENSOR_LINK;
            sensor_link_result_t result = sensor_link_feed(&sensor_parser, byte, now, &ignored);
            return result == SENSOR_LINK_EMPTY ? NOSTOS_EMPTY : NOSTOS_BAD_VALUE;
        }
        return NOSTOS_EMPTY;
    }

    if (uart_route == UART_ROUTE_NOSTOS) {
        uint8_t wire[NOSTOS_WIRE_MAX];
        size_t length = 0U;
        nostos_result_t result = nostos_uart_feed(&nostos_parser, byte, now, wire, &length);
        if (byte == NOSTOS_UART_FLAG || result == NOSTOS_TIMEOUT) uart_route = UART_ROUTE_IDLE;
        return result == NOSTOS_OK ? enqueue_local(wire, length) : result;
    }

    sensor_link_message_t message = {0};
    sensor_link_result_t sensor_result = sensor_link_feed(&sensor_parser, byte, now, &message);
    if (sensor_result == SENSOR_LINK_OK) {
        uart_route = UART_ROUTE_IDLE;
        queue_local_control(&message);
        return NOSTOS_EMPTY;
    }
    if (sensor_result != SENSOR_LINK_EMPTY) {
        uart_route = UART_ROUTE_IDLE;
        return sensor_result == SENSOR_LINK_TIMEOUT ? NOSTOS_TIMEOUT : NOSTOS_BAD_VALUE;
    }
    return NOSTOS_EMPTY;
}

static void uart_task(void *arg)
{
    (void)arg;
    uart_event_t event;
    for (;;) {
        if (xQueueReceive(events, &event, portMAX_DELAY) != pdTRUE) continue;
        if (event.type == UART_DATA) {
            for (size_t i = 0; i < event.size; ++i) {
                uint8_t byte;
                if (uart_read_bytes(DATA_UART, &byte, 1, 0) != 1) break;
                nostos_result_t result = consume_uart_byte(byte);
                if (result != NOSTOS_OK && result != NOSTOS_EMPTY) {
                    ESP_LOGW(TAG, "UART_DROP %s", nostos_result_name(result));
                }
            }
        } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                   event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR) {
            nostos_uart_reset(&nostos_parser);
            sensor_link_reset(&sensor_parser);
            uart_route = UART_ROUTE_IDLE;
            uart_flush_input(DATA_UART);
            xQueueReset(events);
            ESP_LOGW(TAG, "UART_HW_ERROR frame discarded");
        }
        vTaskDelay(1);
    }
}

void bridge_runtime_mesh_rx(const uint8_t *wire, size_t length, uint16_t source,
                            uint16_t own)
{
    uint8_t own_source = source_for_address(own);
    portENTER_CRITICAL(&lock);
    bool bound = identity.bridge_initialized && identity.local_source == own_source &&
                 identity.primary_address == own;
    bool confirmed = identity.identity_confirmed;
    portEXIT_CRITICAL(&lock);
    if (source == own || !worker || own_source == 0U || !bound || !confirmed) return;
    nostos_result_t result = enqueue_remote(wire, length, source);
    ESP_LOGI(TAG, "MESH_RX address=0x%04x len=%u result=%s", source,
             (unsigned)length, nostos_result_name(result));
}

void bridge_runtime_mesh_complete(int error)
{
    portENTER_CRITICAL(&lock);
    if (!stm_boot_reset_in_progress && identity.identity_confirmed) {
        if (error) ++async_failed; else ++async_ok;
    }
    portEXIT_CRITICAL(&lock);
}

esp_err_t bridge_runtime_send_sensor_ride(bool valid, uint16_t kmh_x10,
                                          uint32_t distance_mm)
{
    if (!worker || !ride_queue || !reset_mutex) return ESP_ERR_INVALID_STATE;
    if (!valid && (kmh_x10 != 0U || distance_mm != 0U)) return ESP_ERR_INVALID_ARG;
    sensor_link_ride_t ride = {
        .valid = valid,
        .kmh_x10 = valid ? kmh_x10 : 0U,
        .distance_mm = valid ? distance_mm : 0U,
    };
    if (xSemaphoreTake(reset_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    portENTER_CRITICAL(&lock);
    bool resetting = stm_boot_reset_in_progress;
    bool confirmed = identity.identity_confirmed;
    if (!resetting && !confirmed) ++ride_uart_deferred;
    portEXIT_CRITICAL(&lock);
    BaseType_t queued = resetting ? pdFAIL : xQueueOverwrite(ride_queue, &ride);
    xSemaphoreGive(reset_mutex);
    if (resetting) return ESP_ERR_INVALID_STATE;
    if (queued != pdPASS) return ESP_FAIL;
    xTaskNotifyGive(worker);
    return ESP_OK;
}

void bridge_runtime_log_status(void)
{
    portENTER_CRITICAL(&lock);
    identity_state_t id = identity;
    uint32_t a = accepted, r = rejected, ok = async_ok, bad = async_failed;
    uint32_t ride_ok = ride_uart_ok, ride_bad = ride_uart_failed;
    uint32_t ride_deferred = ride_uart_deferred;
    uint32_t id_ok = identity_uart_ok, id_bad = identity_uart_failed;
    uint32_t approval_ok = approve_uart_ok, approval_bad = approve_uart_failed;
    uint32_t controls = control_received, controls_bad = control_rejected;
    uint32_t controls_full = control_overflow;
    uint32_t epoch = stm_boot_epoch;
    bool resetting = stm_boot_reset_in_progress;
    size_t pending = id.bridge_initialized ? bridge.count : 0U;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,
             "STATUS version=2 primary=0x%04x source=%u session=%" PRIu32
             " confirmed=%u waiting_ack=%u epoch=%" PRIu32 " resetting=%u"
             " pending=%u accepted=%" PRIu32 " rejected=%" PRIu32
             " async_ok_total=%" PRIu32 " async_failed_total=%" PRIu32
             " ride_uart_ok=%" PRIu32 " ride_uart_failed=%" PRIu32
             " ride_uart_deferred=%" PRIu32 " identity_uart_ok=%" PRIu32
             " identity_uart_failed=%" PRIu32 " approve_uart_ok=%" PRIu32
             " approve_uart_failed=%" PRIu32 " control_received=%" PRIu32
             " control_rejected=%" PRIu32 " control_overflow=%" PRIu32,
             id.primary_address, id.local_source, id.session_id, id.identity_confirmed,
             id.identity_waiting_ack, epoch, resetting, (unsigned)pending, a, r, ok, bad,
             ride_ok, ride_bad, ride_deferred, id_ok, id_bad, approval_ok,
             approval_bad, controls, controls_bad, controls_full);
}

esp_err_t bridge_runtime_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(DATA_UART, &cfg);
    if (err != ESP_OK) return err;
    err = uart_set_pin(DATA_UART, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err = uart_driver_install(DATA_UART, 512, 0, 16, &events, 0);
    if (err != ESP_OK) return err;

    reset_mutex = xSemaphoreCreateMutexStatic(&reset_mutex_control);
    if (reset_mutex == NULL) {
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    ride_queue = xQueueCreateStatic(1U, sizeof(sensor_link_ride_t),
                                    ride_queue_storage, &ride_queue_control);
    control_queue = xQueueCreateStatic(CONTROL_QUEUE_LENGTH, sizeof(sensor_link_message_t),
                                       control_queue_storage, &control_queue_control);
    if (ride_queue == NULL || control_queue == NULL) {
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(worker_task, "nostos_v2_tx", 5120, NULL, 5, &worker) != pdPASS) {
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(uart_task, "nostos_v2_rx", 4096, NULL, 4, NULL) != pdPASS) {
        vTaskDelete(worker);
        worker = NULL;
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,
             "UART1_READY version=2 TX=17 RX=18 115200/8N1; identity source is Mesh-bound");
    return ESP_OK;
}
