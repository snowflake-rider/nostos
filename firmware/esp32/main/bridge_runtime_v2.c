/* Explicit v2 bridge. One image derives its source from the provisioned Mesh
 * primary address; UART TX and durable identity writes stay in the worker. */
#include "bridge_runtime.h"
#include "application_event_heap.h"
#include "application_message_engine.h"
#include "mesh_inflight.h"
#include "mesh_retry.h"
#include "mesh_node.h"
#include "nostos_bridge.h"
#include "official_packet_writer.h"
#include "output_command_retry.h"
#include "sensor_link.h"
#include "shared_data_cache.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include <inttypes.h>
#include <string.h>

#define TAG "NOSTOS_V2"
#define DATA_UART UART_NUM_1
#define IDENTITY_NVS_NAMESPACE "nostos_ident"
#define IDENTITY_NVS_KEY "session"
#define CONTROL_QUEUE_LENGTH 4U
#define EVENT_QUEUE_LENGTH 5U
#define STOP_EVENT_QUEUE_LENGTH 5U
#define URGENT_EVENT_QUEUE_LENGTH 4U
#define SENSOR_OUTPUT_SLOT_COUNT (NOSTOS_NODE_COUNT * 2U)
#define IDENTITY_POLL_MS 100U
#define READY_RETRY_MS 1000U
#define MESH_RETRY_INTERVAL_MS 100U

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
    uint8_t local_source;
    uint16_t primary_address;
    uint32_t session_id;
} identity_state_t;

typedef struct {
    nostos_job_t job;
    bool pending;
} cache_snapshot_slot_t;

static nostos_bridge_t bridge;
static nostos_uart_parser_t nostos_parser;
static sensor_link_parser_t sensor_parser;
static uart_route_t uart_route;
static identity_state_t identity;
static official_packet_writer_t official_writer;
static application_message_engine_t app_engine;
static bool boot_session_committed;
static uint32_t boot_session;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static QueueHandle_t events;
static cache_snapshot_slot_t cache_snapshots[SENSOR_OUTPUT_SLOT_COUNT];
static mesh_retry_slot_t mesh_retry;
static uint32_t mesh_retry_next_ms;
static mesh_inflight_t mesh_inflight;
static application_event_heap_t event_outputs;
static output_command_retry_t output_retry;

static StaticQueue_t ride_queue_control;
static uint8_t ride_queue_storage[sizeof(sensor_link_ride_t)];
static QueueHandle_t ride_queue;

static StaticQueue_t environment_queue_control;
static uint8_t environment_queue_storage[sizeof(sensor_link_environment_t)];
static QueueHandle_t environment_queue;

static StaticQueue_t event_queue_control;
static uint8_t event_queue_storage[EVENT_QUEUE_LENGTH * sizeof(sensor_link_event_t)];
static QueueHandle_t event_queue;

static StaticQueue_t stop_event_queue_control;
static uint8_t stop_event_queue_storage[
    STOP_EVENT_QUEUE_LENGTH * sizeof(sensor_link_event_t)];
static QueueHandle_t stop_event_queue;

static StaticQueue_t urgent_event_queue_control;
static uint8_t urgent_event_queue_storage[
    URGENT_EVENT_QUEUE_LENGTH * sizeof(sensor_link_event_t)];
static QueueHandle_t urgent_event_queue;

static StaticQueue_t control_queue_control;
static uint8_t control_queue_storage[CONTROL_QUEUE_LENGTH * sizeof(sensor_link_message_t)];
static QueueHandle_t control_queue;

static uint32_t accepted, rejected, async_ok, async_failed;
static uint32_t local_publish_ok, local_publish_failed;
static uint32_t local_mirror_failed;
static uint32_t ready_uart_ok, ready_uart_failed;
static uint32_t control_received, control_rejected, control_overflow;
static uint32_t event_received, event_overflow, stop_event_overflow;
static uint32_t urgent_event_overflow;
static uint32_t ride_overwrites, environment_overwrites;
static uint32_t cache_hits, cache_misses;
static uint32_t cache_uart_ok, cache_uart_failed;
static bool ready_advertised_once;
static uint32_t ready_last_tx_ms;

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
    /* Mesh completion totals are ESP-uptime counters. An STM boot boundary
     * resets only its mirror and must not reset ESP-owned send accounting. */
    local_publish_ok = 0U;
    local_publish_failed = 0U;
    local_mirror_failed = 0U;
    ready_uart_ok = 0U;
    ready_uart_failed = 0U;
    control_received = 0U;
    control_rejected = 0U;
    control_overflow = 0U;
    event_received = 0U;
    event_overflow = 0U;
    stop_event_overflow = 0U;
    urgent_event_overflow = 0U;
    ride_overwrites = 0U;
    environment_overwrites = 0U;
    cache_hits = 0U;
    cache_misses = 0U;
    cache_uart_ok = 0U;
    cache_uart_failed = 0U;
    ready_advertised_once = false;
    ready_last_tx_ms = 0U;
}

static uint8_t source_for_address(uint16_t address)
{
    if (address == 0U) return 0U;
    for (size_t i = 0; i < NOSTOS_NODE_COUNT; ++i) {
        if (peers[i].mesh_address == address) return peers[i].source_id;
    }
    return 0U;
}

static bool display_event_type(uint8_t type)
{
    return type == NOSTOS_FALL || type == NOSTOS_FALL_CLEAR ||
        type == NOSTOS_STOP || type == NOSTOS_SPEED_UP ||
        type == NOSTOS_SPEED_DOWN;
}

/* Caller holds lock. Accepted official packets are kept only inside ESP and
 * become local OUTPUT_* work; official 0x7E frames never cross to STM. */
static nostos_result_t schedule_output_message_locked(
    const nostos_message_t *message,
    uint32_t received_ms)
{
    if (message == NULL) return NOSTOS_BAD_ARGUMENT;
    nostos_message_t captured;
    nostos_result_t captured_result =
        application_message_engine_capture_output(
            &app_engine, message, &captured);
    if (captured_result != NOSTOS_OK) return captured_result;
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    nostos_result_t encoded = nostos_message_encode(
        &captured, wire, sizeof(wire), &length);
    if (encoded != NOSTOS_OK) return encoded;
    if (display_event_type(captured.type)) {
        return application_event_heap_push(
            &event_outputs, wire, length, received_ms);
    }
    size_t type_slot;
    if (captured.type == NOSTOS_RIDE) {
        type_slot = 0U;
    } else if (captured.type == NOSTOS_ENVIRONMENT) {
        type_slot = 1U;
    } else {
        return NOSTOS_UNSUPPORTED_TYPE;
    }
    size_t slot = ((size_t)captured.source_id - 1U) * 2U + type_slot;
    cache_snapshots[slot] = (cache_snapshot_slot_t){
        .job = {
            .length = length,
            .received_ms = received_ms,
            .direction = NOSTOS_TO_UART,
        },
        .pending = true,
    };
    memcpy(cache_snapshots[slot].job.wire, wire, length);
    return NOSTOS_OK;
}

static bool bridge_ready(void)
{
    uint8_t source;
    uint16_t primary;
    bool initialized;
    portENTER_CRITICAL(&lock);
    source = identity.local_source;
    primary = identity.primary_address;
    initialized = identity.bridge_initialized;
    portEXIT_CRITICAL(&lock);
    return initialized && source != 0U &&
           source_for_address(primary) == source &&
           mesh_node_ready() && mesh_node_primary() == primary;
}

static bool uart_write(const uint8_t *bytes, size_t length);

static bool local_writer_ready(void)
{
    portENTER_CRITICAL(&lock);
    bool ready = identity.bridge_initialized && identity.identity_loaded &&
        identity.session_id != 0U && official_writer.initialized &&
        official_writer.sender.source_id == identity.local_source &&
        official_writer.sender.session_id == identity.session_id;
    portEXIT_CRITICAL(&lock);
    return ready;
}

/* Worker-only. A new ESP boot epoch is announced without waiting for STM
 * HELLO, and no OUTPUT command is dispatched until this write succeeds. */
static bool advertise_ready(bool force)
{
    uint32_t now = now_ms();
    portENTER_CRITICAL(&lock);
    bool bound = identity.bridge_initialized && identity.identity_loaded &&
        identity.session_id != 0U;
    bool confirmed = identity.identity_confirmed;
    uint32_t epoch = identity.session_id;
    portEXIT_CRITICAL(&lock);
    if (!bound || (!force && confirmed)) return false;
    if (!force && ready_advertised_once &&
        (uint32_t)(now - ready_last_tx_ms) < READY_RETRY_MS) {
        return false;
    }

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t frame_length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_ready(
        epoch, frame, &frame_length);
    bool sent = encoded == SENSOR_LINK_OK && uart_write(frame, frame_length);
    portENTER_CRITICAL(&lock);
    identity.identity_confirmed = sent;
    identity.identity_waiting_ack = false;
    ready_advertised_once = true;
    ready_last_tx_ms = now;
    portEXIT_CRITICAL(&lock);
    counter_increment(sent ? &ready_uart_ok : &ready_uart_failed);
    ESP_LOGI(TAG, "READY_TX epoch=%" PRIu32 " api=%s",
             epoch, sent ? "accepted" : "failed");
    return true;
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
        ready_advertised_once = false;
        ready_last_tx_ms = 0U;
    }
    portEXIT_CRITICAL(&lock);
    if (already_bound) return true;
    if (source == 0U) return false;

    uint32_t stored_session = boot_session;
    if (!boot_session_committed) {
        esp_err_t err = nvs_load_session(&stored_session);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "IDENTITY_LOAD_FAILED primary=0x%04x err=%s",
                     primary, esp_err_to_name(err));
            return false;
        }
        uint32_t next_session = 0U;
        err = nvs_commit_next_session(stored_session, &next_session);
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "IDENTITY_BOOT_ADVANCE_FAILED source=%u current=%" PRIu32
                     " err=%s",
                     source, stored_session, esp_err_to_name(err));
            return false;
        }
        nostos_result_t writer_result = official_packet_writer_init(
            &official_writer, source, next_session);
        if (writer_result != NOSTOS_OK) {
            ESP_LOGE(TAG, "WRITER_INIT_FAILED result=%s",
                     nostos_result_name(writer_result));
            return false;
        }
        boot_session = next_session;
        boot_session_committed = true;
    } else if (official_packet_writer_set_source(
                   &official_writer, source) != NOSTOS_OK) {
        ESP_LOGE(TAG, "WRITER_REBIND_FAILED source=%u", source);
        return false;
    }

    application_message_engine_t initialized_engine;
    nostos_result_t engine_result = application_message_engine_init(
        &initialized_engine, source, boot_session);
    if (engine_result != NOSTOS_OK) {
        ESP_LOGE(TAG, "APP_ENGINE_INIT_FAILED result=%s",
                 nostos_result_name(engine_result));
        return false;
    }

    /* Recheck after the I/O: a provisioning reset may have changed identity. */
    if (mesh_node_primary() != primary) return false;
    portENTER_CRITICAL(&lock);
    nostos_result_t result = nostos_bridge_init(&bridge, source, peers);
    if (result == NOSTOS_OK) {
        app_engine = initialized_engine;
        application_event_heap_init(&event_outputs);
        output_command_retry_init(&output_retry);
        for (size_t i = 0U; i < SENSOR_OUTPUT_SLOT_COUNT; ++i) {
            cache_snapshots[i] = (cache_snapshot_slot_t){0};
        }
        identity = (identity_state_t){
            .bridge_initialized = true,
            .identity_loaded = true,
            .identity_confirmed = false,
            .identity_waiting_ack = false,
            .local_source = source,
            .primary_address = primary,
            .session_id = boot_session,
        };
        ready_advertised_once = false;
        ready_last_tx_ms = 0U;
    }
    portEXIT_CRITICAL(&lock);
    if (result != NOSTOS_OK) {
        ESP_LOGE(TAG, "IDENTITY_BIND_FAILED result=%s", nostos_result_name(result));
        return false;
    }
    ESP_LOGI(TAG,
             "IDENTITY_BOUND primary=0x%04x source=%u boot_session=%" PRIu32,
             primary, source, boot_session);
    return true;
}

static bool process_local_control(void)
{
    sensor_link_message_t message;
    if (control_queue == NULL || xQueueReceive(control_queue, &message, 0) != pdTRUE) return false;

    if (message.type == SENSOR_LINK_HELLO) {
        if (!refresh_identity_binding()) {
            counter_increment(&ready_uart_failed);
            ESP_LOGW(TAG, "READY_DEFERRED no verified Mesh primary");
            return true;
        }
        portENTER_CRITICAL(&lock);
        identity.identity_confirmed = false;
        portEXIT_CRITICAL(&lock);
        (void)advertise_ready(true);
        nostos_message_t snapshots[APPLICATION_MESSAGE_SNAPSHOT_CAPACITY];
        size_t snapshot_count = 0U;
        uint32_t hello_now = now_ms();
        portENTER_CRITICAL(&lock);
        bool sent = identity.identity_confirmed;
        if (sent && application_message_engine_snapshot(
                &app_engine, hello_now, snapshots,
                &snapshot_count) == NOSTOS_OK) {
            for (size_t i = 0U; i < snapshot_count; ++i) {
                nostos_result_t scheduled = schedule_output_message_locked(
                    &snapshots[i], hello_now);
                if (scheduled == NOSTOS_OK) {
                    ++cache_hits;
                } else {
                    ++cache_misses;
                }
            }
        }
        portEXIT_CRITICAL(&lock);
        if (sent && snapshot_count != 0U) xTaskNotifyGive(worker);
        ESP_LOGI(TAG, "HELLO_RESYNC api=%s snapshots=%u",
                 sent ? "accepted" : "failed", (unsigned)snapshot_count);
        return true;
    }

    if (message.type == SENSOR_LINK_OUTPUT_RESULT) {
        portENTER_CRITICAL(&lock);
        nostos_result_t result = application_message_engine_note_output_result(
            &app_engine, &message.output_result);
        portEXIT_CRITICAL(&lock);
        ESP_LOGI(TAG, "OUTPUT_RESULT command_id=%" PRIu32
                 " status=%u result=%s",
                 message.output_result.command_id,
                 (unsigned)message.output_result.status,
                 nostos_result_name(result));
        return true;
    }

    if (message.type == SENSOR_LINK_SHARED_DATA_REQUEST) {
        nostos_message_t snapshots[APPLICATION_MESSAGE_SNAPSHOT_CAPACITY];
        size_t snapshot_count = 0U;
        uint8_t requested = message.shared_data_request.mask;
        bool scheduled = false;
        uint32_t request_now = now_ms();
        portENTER_CRITICAL(&lock);
        if (identity.bridge_initialized && identity.identity_confirmed) {
            nostos_result_t snapshot_result =
                application_message_engine_snapshot(
                    &app_engine, request_now, snapshots, &snapshot_count);
            if (snapshot_result == NOSTOS_OK) {
                for (size_t i = 0U; i < snapshot_count; ++i) {
                    uint8_t bit = snapshots[i].type == NOSTOS_RIDE
                        ? NOSTOS_SHARED_DATA_RIDE
                        : snapshots[i].type == NOSTOS_ENVIRONMENT
                            ? NOSTOS_SHARED_DATA_ENVIRONMENT : 0U;
                    if ((requested & bit) == 0U) continue;
                    nostos_result_t output_result =
                        schedule_output_message_locked(
                            &snapshots[i], request_now);
                    if (output_result == NOSTOS_OK) {
                        ++cache_hits;
                        scheduled = true;
                    } else {
                        ++cache_misses;
                    }
                }
            }
        } else {
            ++control_rejected;
        }
        portEXIT_CRITICAL(&lock);
        if (scheduled) xTaskNotifyGive(worker);
        return true;
    }

    counter_increment(&control_rejected);
    ESP_LOGW(TAG, "LOCAL_CONTROL_REJECT type=0x%02x", message.type);
    return true;
}

static nostos_result_t enqueue_remote(const uint8_t *wire, size_t length,
                                      uint16_t mesh_source)
{
    nostos_message_t decoded;
    nostos_result_t validation = nostos_message_decode(wire, length, &decoded);
    /* Snapshot requests are paired-UART commands. Never forward a request
     * received from Mesh back into STM, which could create reply fan-out. */
    if (validation == NOSTOS_OK &&
        decoded.type == NOSTOS_SHARED_DATA_REQUEST) {
        return NOSTOS_UNSUPPORTED_TYPE;
    }
    bool authenticated = validation == NOSTOS_OK &&
        source_for_address(mesh_source) == decoded.source_id;
    if (validation != NOSTOS_OK) return validation;
    if (!authenticated) return NOSTOS_UNAUTHORIZED;
    uint32_t now = now_ms();
    bool queued = false;
    portENTER_CRITICAL(&lock);
    nostos_result_t result = NOSTOS_NOT_READY;
    if (identity.bridge_initialized) {
        const nostos_rx_window_t *window =
            &app_engine.receiver.windows[decoded.source_id - 1U];
        bool new_session = !window->approved ||
            decoded.session_id > window->session_id;
        result = application_message_engine_approve_authenticated_session(
            &app_engine, decoded.source_id, decoded.session_id,
            decoded.sequence);
        if (result == NOSTOS_OK) {
            if (new_session) {
                (void)application_event_heap_discard_source_before_session(
                    &event_outputs, decoded.source_id, decoded.session_id);
                (void)output_command_retry_discard_source_before_session(
                    &output_retry, decoded.source_id, decoded.session_id);
                size_t first_sensor_slot =
                    ((size_t)decoded.source_id - 1U) * 2U;
                cache_snapshots[first_sensor_slot] =
                    (cache_snapshot_slot_t){0};
                cache_snapshots[first_sensor_slot + 1U] =
                    (cache_snapshot_slot_t){0};
            }
            nostos_message_t accepted_message;
            result = application_message_engine_accept_wire(
                &app_engine, wire, length, now, &accepted_message);
            if (result == NOSTOS_OK) {
                result = schedule_output_message_locked(
                    &accepted_message, now);
                queued = result == NOSTOS_OK;
            }
        }
    }
    if (result == NOSTOS_OK) ++accepted; else ++rejected;
    portEXIT_CRITICAL(&lock);
    if (queued) xTaskNotifyGive(worker);
    return result;
}

/* Worker-only: apply one ESP-stamped packet to the same receiver used by Mesh,
 * then queue its OUTPUT_* mirror and Mesh transport independently. */
static nostos_result_t route_local_official(
    const uint8_t *wire,
    size_t length)
{
    nostos_message_t decoded;
    nostos_result_t validation = nostos_message_decode(wire, length, &decoded);
    if (validation != NOSTOS_OK) return validation;
    bool ready = bridge_ready();
    uint32_t now = now_ms();
    bool worker_wakeup = false;
    portENTER_CRITICAL(&lock);
    nostos_result_t result = NOSTOS_SESSION_REQUIRED;
    if (identity.bridge_initialized && identity.identity_loaded &&
        decoded.source_id == identity.local_source &&
        decoded.session_id == identity.session_id) {
        nostos_message_t accepted_message;
        result = application_message_engine_accept_wire(
            &app_engine, wire, length, now, &accepted_message);
        if (result == NOSTOS_OK) {
            nostos_result_t output_result = schedule_output_message_locked(
                &accepted_message, now);
            if (output_result == NOSTOS_OK) {
                worker_wakeup = true;
            } else {
                ++local_mirror_failed;
                ESP_LOGW(TAG, "OUTPUT_QUEUE_FAILED type=0x%02x result=%s",
                         decoded.type, nostos_result_name(output_result));
            }

            nostos_result_t mesh_result = nostos_bridge_accept(
                &bridge, NOSTOS_TO_MESH, wire, length, 0U, now, ready);
            if (mesh_result == NOSTOS_OK) {
                worker_wakeup = true;
            } else {
                nostos_job_t retry_job = {
                    .length = length,
                    .received_ms = now,
                    .direction = NOSTOS_TO_MESH,
                };
                memcpy(retry_job.wire, wire, length);
                if (mesh_retry_store(&mesh_retry, &retry_job) == NOSTOS_OK) {
                    mesh_retry_next_ms = now + MESH_RETRY_INTERVAL_MS;
                    worker_wakeup = true;
                } else {
                    ++local_publish_failed;
                }
                ESP_LOGW(TAG, "MESH_ENQUEUE_FAILED type=0x%02x result=%s",
                         decoded.type, nostos_result_name(mesh_result));
            }
        }
    }
    if (result == NOSTOS_OK) ++accepted; else ++rejected;
    portEXIT_CRITICAL(&lock);
    if (worker_wakeup) xTaskNotifyGive(worker);
    return result;
}

static bool process_sensor_ride(void)
{
    sensor_link_ride_t ride;
    if (!local_writer_ready()) return false;
    if (ride_queue == NULL ||
        xQueueReceive(ride_queue, &ride, 0) != pdTRUE) return false;
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    nostos_result_t encoded = official_packet_writer_ride(
        &official_writer, ride.valid, ride.kmh_x10, ride.distance_mm,
        wire, &length);
    nostos_result_t result = encoded == NOSTOS_OK
        ? route_local_official(wire, length) : encoded;
    counter_increment(result == NOSTOS_OK ?
        &local_publish_ok : &local_publish_failed);
    ESP_LOGI(TAG, "RIDE_PUBLISH valid=%u kmh_x10=%u distance_mm=%" PRIu32
             " result=%s",
             ride.valid, (unsigned)ride.kmh_x10, ride.distance_mm,
             nostos_result_name(result));
    return true;
}

static bool process_environment(void)
{
    sensor_link_environment_t environment;
    if (!local_writer_ready()) return false;
    if (environment_queue == NULL ||
        xQueueReceive(environment_queue, &environment, 0) != pdTRUE) {
        return false;
    }
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    nostos_result_t encoded = official_packet_writer_environment(
        &official_writer,
        environment.temperature_c_x10,
        environment.humidity_pct_x10,
        (nostos_quality_t)environment.temperature_quality,
        (nostos_quality_t)environment.humidity_quality,
        wire, &length);
    nostos_result_t result = encoded == NOSTOS_OK
        ? route_local_official(wire, length) : encoded;
    counter_increment(result == NOSTOS_OK ?
        &local_publish_ok : &local_publish_failed);
    ESP_LOGI(TAG, "ENVIRONMENT_PUBLISH temp_x10=%d humidity_x10=%u result=%s",
             (int)environment.temperature_c_x10,
             (unsigned)environment.humidity_pct_x10,
             nostos_result_name(result));
    return true;
}

static void retain_mesh_retry(const nostos_job_t *job, uint32_t now)
{
    if (mesh_retry_store(&mesh_retry, job) == NOSTOS_OK) {
        mesh_retry_next_ms = now + MESH_RETRY_INTERVAL_MS;
    }
}

static bool process_event(QueueHandle_t queue, bool *blocked)
{
    if (blocked != NULL) *blocked = false;
    sensor_link_event_t event;
    if (queue == NULL || xQueuePeek(queue, &event, 0) != pdTRUE) return false;
    if (!local_writer_ready()) {
        if (blocked != NULL) *blocked = true;
        return false;
    }
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    nostos_result_t encoded = official_packet_writer_event(
        &official_writer, event.type, wire, &length);
    nostos_result_t result = encoded == NOSTOS_OK
        ? route_local_official(wire, length) : encoded;
    sensor_link_event_t consumed;
    if (xQueueReceive(queue, &consumed, 0) != pdTRUE) {
        ESP_LOGE(TAG, "EVENT_DEQUEUE_FAILED type=0x%02x", event.type);
    }
    counter_increment(result == NOSTOS_OK ?
        &local_publish_ok : &local_publish_failed);
    ESP_LOGI(TAG, "EVENT_PUBLISH type=0x%02x result=%s",
             event.type, nostos_result_name(result));
    return true;
}

static bool process_cache_snapshot(void)
{
    bool local_priority_pending =
        (urgent_event_queue != NULL &&
         uxQueueMessagesWaiting(urgent_event_queue) != 0U) ||
        (stop_event_queue != NULL &&
         uxQueueMessagesWaiting(stop_event_queue) != 0U) ||
        (event_queue != NULL &&
         uxQueueMessagesWaiting(event_queue) != 0U);
    if (local_priority_pending) {
        return false;
    }
    bool mesh_retry_safety = mesh_retry.pending &&
        (mesh_retry_job_is_fall(&mesh_retry.job) ||
         (mesh_retry.job.length >= NOSTOS_HEADER_SIZE &&
          mesh_retry.job.wire[1] == NOSTOS_STOP));
    if (mesh_retry_safety) return false;
    cache_snapshot_slot_t selected_slot = {0};
    bool found = false;
    size_t selected = 0U;
    portENTER_CRITICAL(&lock);
    if (identity.identity_confirmed &&
        !output_command_retry_pending(&output_retry) &&
        bridge.urgent_count == 0U &&
        bridge.stop_count == 0U && event_outputs.count == 0U) {
        for (size_t i = 0U; i < SENSOR_OUTPUT_SLOT_COUNT; ++i) {
            if (cache_snapshots[i].pending) {
                selected_slot = cache_snapshots[i];
                cache_snapshots[i].pending = false;
                selected = i;
                found = true;
                break;
            }
        }
    }
    portEXIT_CRITICAL(&lock);
    if (!found) return false;
    local_priority_pending =
        uxQueueMessagesWaiting(urgent_event_queue) != 0U ||
        uxQueueMessagesWaiting(stop_event_queue) != 0U ||
        uxQueueMessagesWaiting(event_queue) != 0U;
    if (local_priority_pending) {
        portENTER_CRITICAL(&lock);
        cache_snapshots[selected] = selected_slot;
        portEXIT_CRITICAL(&lock);
        return false;
    }

    nostos_message_t captured;
    nostos_result_t decoded = nostos_message_decode(
        selected_slot.job.wire, selected_slot.job.length, &captured);
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t frame_length = 0U;
    uint32_t command_id = 0U;
    portENTER_CRITICAL(&lock);
    nostos_result_t output_result = decoded == NOSTOS_OK
        ? application_message_engine_encode_captured_output(
            &app_engine, &captured, frame, &frame_length, &command_id)
        : decoded;
    portEXIT_CRITICAL(&lock);
    bool sent = output_result == NOSTOS_OK && uart_write(frame, frame_length);
    uint32_t retry_now = now_ms();
    portENTER_CRITICAL(&lock);
    application_message_engine_note_uart_tx(&app_engine, sent);
    nostos_result_t retained = !sent && output_result == NOSTOS_OK
        ? output_command_retry_store(
            &output_retry, frame, frame_length, command_id,
            captured.session_id, captured.type, captured.source_id, retry_now)
        : NOSTOS_EMPTY;
    portEXIT_CRITICAL(&lock);
    counter_increment(sent ? &cache_uart_ok : &cache_uart_failed);
    if (!sent && output_result == NOSTOS_OK && retained != NOSTOS_OK) {
        ESP_LOGE(TAG, "CACHE_OUTPUT_RETAIN_FAILED command_id=%" PRIu32
                 " result=%s", command_id, nostos_result_name(retained));
    }
    ESP_LOGI(TAG, "CACHE_OUTPUT_TX command_id=%" PRIu32
             " type=0x%02x source=%u api=%s result=%s",
             command_id, selected_slot.job.wire[1],
             selected_slot.job.wire[2], sent ? "accepted" : "failed",
             nostos_result_name(output_result));
    return true;
}

static bool mesh_send_active(void)
{
    portENTER_CRITICAL(&lock);
    bool active = mesh_inflight_active(&mesh_inflight);
    portEXIT_CRITICAL(&lock);
    return active;
}

static uint8_t bridge_priority_locked(void)
{
    if (bridge.urgent_count != 0U) return 4U;
    if (bridge.stop_count != 0U) return 3U;
    return bridge.normal_count != 0U ? 1U : 0U;
}

static uint8_t event_output_rank_locked(void)
{
    uint8_t priority = application_event_heap_priority(&event_outputs);
    return priority == 0U ? 0U : (uint8_t)(5U - priority);
}

static uint8_t mesh_retry_priority(void)
{
    if (!mesh_retry.pending) return 0U;
    if (mesh_retry_job_is_fall(&mesh_retry.job)) return 4U;
    return mesh_retry.job.length >= NOSTOS_HEADER_SIZE &&
        mesh_retry.job.wire[1] == NOSTOS_STOP ? 3U : 1U;
}

/* Worker-only. A failed UART write may already have reached STM, so retry the
 * exact frame and command_id before allocating or dispatching a later id. */
static bool process_output_retry(uint32_t now, nostos_result_t *result)
{
    if (result == NULL) return false;
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t frame_length = 0U;
    uint32_t command_id = 0U;
    uint8_t message_type = 0U;
    uint8_t source_id = 0U;
    portENTER_CRITICAL(&lock);
    *result = output_command_retry_peek(
        &output_retry, identity.identity_confirmed, now,
        frame, &frame_length, &command_id, &message_type, &source_id);
    portEXIT_CRITICAL(&lock);
    if (*result == NOSTOS_EMPTY) return false;
    if (*result != NOSTOS_OK) return true;

    bool sent = uart_write(frame, frame_length);
    uint32_t completed_ms = now_ms();
    portENTER_CRITICAL(&lock);
    application_message_engine_note_uart_tx(&app_engine, sent);
    nostos_result_t finished = output_command_retry_finish(
        &output_retry, command_id, sent, completed_ms);
    portEXIT_CRITICAL(&lock);
    if (!display_event_type(message_type)) {
        counter_increment(sent ? &cache_uart_ok : &cache_uart_failed);
    }
    *result = sent && finished == NOSTOS_OK ? NOSTOS_OK :
        finished != NOSTOS_OK ? finished : NOSTOS_IO_ERROR;
    ESP_LOGI(TAG, "OUTPUT_RETRY_TX command_id=%" PRIu32
             " type=0x%02x source=%u api=%s result=%s",
             command_id, message_type, source_id,
             sent ? "accepted" : "failed", nostos_result_name(*result));
    return true;
}

/* Worker-only. Local and remote accepted events share this one priority queue;
 * STM READY gates dispatch but never owns official sessions or dedup state. */
static bool process_remote_event(uint32_t now, nostos_result_t *result)
{
    if (result == NULL) return false;
    uint8_t retry_priority = mesh_retry_priority();
    nostos_job_t job = {0};
    portENTER_CRITICAL(&lock);
    uint8_t event_rank = identity.identity_confirmed
        ? event_output_rank_locked() : 0U;
    uint8_t queued_priority = bridge_priority_locked();
    bool selected = event_rank != 0U &&
        event_rank >= retry_priority &&
        event_rank >= queued_priority;
    *result = selected
        ? application_event_heap_pop(&event_outputs, now, &job)
        : NOSTOS_EMPTY;
    portEXIT_CRITICAL(&lock);
    if (!selected) return false;
    if (*result == NOSTOS_EXPIRED) {
        ESP_LOGW(TAG, "REMOTE_EVENT_EXPIRED type=0x%02x source=%u",
                 job.wire[1], job.wire[2]);
        return true;
    }
    if (*result != NOSTOS_OK) return true;
    uint8_t selected_rank = job.wire[1] == NOSTOS_FALL ||
        job.wire[1] == NOSTOS_FALL_CLEAR ? 4U :
        job.wire[1] == NOSTOS_STOP ? 3U :
        job.wire[1] == NOSTOS_SPEED_DOWN ? 2U : 1U;
    portENTER_CRITICAL(&lock);
    uint8_t newer_rank = event_output_rank_locked();
    if (newer_rank > selected_rank) {
        nostos_result_t restored = application_event_heap_push(
            &event_outputs, job.wire, job.length, job.received_ms);
        portEXIT_CRITICAL(&lock);
        *result = restored == NOSTOS_OK ? NOSTOS_NOT_READY : restored;
        return true;
    }
    portEXIT_CRITICAL(&lock);
    nostos_message_t captured;
    nostos_result_t decoded = nostos_message_decode(
        job.wire, job.length, &captured);
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t frame_length = 0U;
    uint32_t command_id = 0U;
    portENTER_CRITICAL(&lock);
    nostos_result_t output_result = decoded == NOSTOS_OK
        ? application_message_engine_encode_captured_output(
            &app_engine, &captured, frame, &frame_length, &command_id)
        : decoded;
    portEXIT_CRITICAL(&lock);
    bool sent = output_result == NOSTOS_OK && uart_write(frame, frame_length);
    uint32_t retry_now = now_ms();
    portENTER_CRITICAL(&lock);
    application_message_engine_note_uart_tx(&app_engine, sent);
    nostos_result_t retained = !sent && output_result == NOSTOS_OK
        ? output_command_retry_store(
            &output_retry, frame, frame_length, command_id,
            captured.session_id, captured.type, captured.source_id, retry_now)
        : NOSTOS_EMPTY;
    portEXIT_CRITICAL(&lock);
    if (!sent && output_result == NOSTOS_OK && retained != NOSTOS_OK) {
        ESP_LOGE(TAG, "EVENT_OUTPUT_RETAIN_FAILED command_id=%" PRIu32
                 " result=%s", command_id, nostos_result_name(retained));
    }
    *result = sent ? NOSTOS_OK :
        output_result == NOSTOS_OK ? NOSTOS_IO_ERROR : output_result;
    ESP_LOGI(TAG,
             "EVENT_OUTPUT_TX command_id=%" PRIu32
             " type=0x%02x source=%u api=%s result=%s",
             command_id, job.wire[1], job.wire[2],
             sent ? "accepted" : "failed",
             nostos_result_name(output_result));
    return true;
}

/* Worker-only. The exact job becomes identifiable before the Mesh API can
 * invoke a fast completion callback. */
static nostos_result_t start_mesh_send(
    const nostos_job_t *job,
    uint32_t now,
    bool from_retry)
{
    portENTER_CRITICAL(&lock);
    nostos_result_t begun = mesh_inflight_begin(&mesh_inflight, job);
    portEXIT_CRITICAL(&lock);
    if (begun != NOSTOS_OK) return begun;

    if (from_retry) {
        mesh_retry_complete(&mesh_retry);
        mesh_retry_next_ms = 0U;
    }
    esp_err_t admitted = mesh_node_send_event(job->wire, job->length);
    if (admitted == ESP_OK) return NOSTOS_OK;

    nostos_job_t cancelled = {0};
    portENTER_CRITICAL(&lock);
    nostos_result_t cancelled_result = mesh_inflight_cancel(
        &mesh_inflight, &cancelled);
    portEXIT_CRITICAL(&lock);
    if (cancelled_result == NOSTOS_OK) {
        retain_mesh_retry(&cancelled, now);
        return NOSTOS_IO_ERROR;
    }
    /* A completion raced the admission return. Let the worker consume that
     * authoritative callback instead of retrying a possibly delivered wire. */
    return cancelled_result == NOSTOS_CONFLICT ? NOSTOS_OK : NOSTOS_IO_ERROR;
}

static nostos_result_t process_one(void)
{
    bool ready = bridge_ready();
    uint32_t now = now_ms();

    nostos_job_t job = {0};
    int completion_error = 0;
    portENTER_CRITICAL(&lock);
    nostos_result_t completion = mesh_inflight_take_completion(
        &mesh_inflight, &job, &completion_error);
    portEXIT_CRITICAL(&lock);
    if (completion == NOSTOS_OK) {
        if (completion_error != 0) {
            retain_mesh_retry(&job, now);
            ESP_LOGW(TAG, "MESH_COMPLETE_RETRY type=0x%02x error=%d",
                     job.wire[1], completion_error);
            return NOSTOS_IO_ERROR;
        }
        ESP_LOGI(TAG, "MESH_COMPLETE type=0x%02x source=%u",
                 job.wire[1], job.wire[2]);
        return NOSTOS_OK;
    }
    if (completion != NOSTOS_EMPTY) return completion;
    nostos_result_t output_retry_result = NOSTOS_EMPTY;
    if (process_output_retry(now, &output_retry_result)) {
        return output_retry_result;
    }
    nostos_result_t remote_result = NOSTOS_EMPTY;
    if (process_remote_event(now, &remote_result)) return remote_result;
    if (mesh_send_active()) return NOSTOS_NOT_READY;

    nostos_result_t retry_result = mesh_retry_peek(&mesh_retry, now, &job);
    if (retry_result == NOSTOS_OK) {
        portENTER_CRITICAL(&lock);
        bool bridge_has_fall = bridge.urgent_count != 0U;
        bool bridge_has_stop = bridge.stop_count != 0U;
        portEXIT_CRITICAL(&lock);
        bool retry_is_stop = job.wire[1] == NOSTOS_STOP;
        bool bridge_has_higher_priority = !mesh_retry_job_is_fall(&job) &&
            (bridge_has_fall || (!retry_is_stop && bridge_has_stop));
        /* A retained job cannot delay a newly queued higher class. The higher
         * job replaces it if that immediate send also fails. */
        if (!bridge_has_higher_priority) {
            if (!ready) return NOSTOS_NOT_READY;
            if ((int32_t)(now - mesh_retry_next_ms) < 0) {
                return NOSTOS_NOT_READY;
            }
            nostos_result_t sent = start_mesh_send(&job, now, true);
            ESP_LOGI(TAG, "MESH_RETRY type=0x%02x source=%u len=%u result=%s",
                     job.wire[1], job.wire[2], (unsigned)job.length,
                     nostos_result_name(sent));
            return sent;
        }
    } else if (retry_result != NOSTOS_EMPTY &&
               retry_result != NOSTOS_EXPIRED) {
        return retry_result;
    }

    portENTER_CRITICAL(&lock);
    nostos_result_t result = identity.bridge_initialized
        ? nostos_bridge_next(&bridge, now, ready, &job)
        : NOSTOS_EMPTY;
    portEXIT_CRITICAL(&lock);
    if (result == NOSTOS_EMPTY) return result;
    if (result == NOSTOS_NOT_READY && job.direction == NOSTOS_TO_MESH) {
        retain_mesh_retry(&job, now);
        ESP_LOGW(TAG, "MESH_RETRY_RETAIN type=0x%02x result=%s",
                 job.wire[1], nostos_result_name(result));
        return result;
    }
    if (result == NOSTOS_EXPIRED && job.direction == NOSTOS_TO_MESH &&
        mesh_retry_job_is_fall(&job)) {
        /* The common bridge TTL applies to normal work only at this layer;
         * safety incidents are retained until an immediate send succeeds. */
        result = ready ? NOSTOS_OK : NOSTOS_NOT_READY;
        if (result == NOSTOS_NOT_READY) {
            retain_mesh_retry(&job, now);
            return result;
        }
    }
    if (result != NOSTOS_OK) {
        ESP_LOGW(TAG, "TX_JOB_DROP result=%s", nostos_result_name(result));
        return result;
    }

    if (job.direction != NOSTOS_TO_MESH) {
        ESP_LOGE(TAG, "OFFICIAL_UART_JOB_REJECTED type=0x%02x", job.wire[1]);
        return NOSTOS_UNAUTHORIZED;
    }
    nostos_result_t mesh_result = start_mesh_send(&job, now, false);
    bool sent = mesh_result == NOSTOS_OK;
    ESP_LOGI(TAG, "MESH_TX type=0x%02x source=%u len=%u api=%s",
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
            bool ready_processed = advertise_ready(false);
            bool control_processed = process_local_control();
            bool urgent_event_blocked = false;
            bool stop_event_blocked = false;
            bool event_blocked = false;
            bool urgent_event_processed = false;
            bool stop_event_processed = false;
            bool event_processed = false;
            while (process_event(urgent_event_queue, &urgent_event_blocked)) {
                urgent_event_processed = true;
            }
            if (!urgent_event_blocked) {
                while (process_event(stop_event_queue, &stop_event_blocked)) {
                    stop_event_processed = true;
                }
            }
            if (!urgent_event_blocked && !stop_event_blocked) {
                while (process_event(event_queue, &event_blocked)) {
                    event_processed = true;
                }
            }
            /* Every accepted local/remote event is now in one min-heap before
             * any output dispatch, so BTN2 can outrank an earlier BTN1. */
            bool ride_processed = !urgent_event_blocked &&
                !stop_event_blocked && !event_blocked && process_sensor_ride();
            bool environment_processed = !urgent_event_blocked &&
                !stop_event_blocked && !event_blocked && process_environment();
            nostos_result_t result = process_one();
            /* Drain every queued FALL/FALL_CLEAR before cache recovery. Once
             * urgent traffic is empty, one snapshot per loop prevents normal
             * bridge traffic from starving recovery. */
            bool snapshot_processed = process_cache_snapshot();
            bool event_retry_blocked = urgent_event_blocked ||
                stop_event_blocked || event_blocked;
            if (event_retry_blocked && result != NOSTOS_OK) break;
            if (mesh_retry.pending &&
                (result == NOSTOS_NOT_READY || result == NOSTOS_IO_ERROR)) {
                break;
            }
            /* Exactly one Mesh job may await an asynchronous completion. The
             * callback wakes this worker to retire or retain that exact wire. */
            if (result == NOSTOS_NOT_READY && mesh_send_active()) break;
            portENTER_CRITICAL(&lock);
            bool output_retry_blocked =
                output_command_retry_pending(&output_retry);
            portEXIT_CRITICAL(&lock);
            if (output_retry_blocked && result != NOSTOS_OK) break;
            if (!ready_processed && !control_processed && result == NOSTOS_EMPTY &&
                !snapshot_processed && !urgent_event_processed &&
                !stop_event_processed && !event_processed && !ride_processed &&
                !environment_processed) break;
            vTaskDelay(1);
        }
    }
}

static void queue_local_control(const sensor_link_message_t *message)
{
    if (message == NULL) {
        counter_increment(&control_rejected);
        return;
    }

    BaseType_t queued = pdFALSE;
    if (message->type == SENSOR_LINK_HELLO ||
        message->type == SENSOR_LINK_SHARED_DATA_REQUEST ||
        message->type == SENSOR_LINK_OUTPUT_RESULT) {
        queued = xQueueSend(control_queue, message, 0);
        if (queued == pdTRUE) {
            counter_increment(&control_received);
        } else {
            counter_increment(&control_overflow);
        }
    } else if (message->type == SENSOR_LINK_EVENT) {
        bool urgent = message->event.type == NOSTOS_FALL ||
            message->event.type == NOSTOS_FALL_CLEAR;
        bool stop = message->event.type == NOSTOS_STOP;
        QueueHandle_t queue = urgent ? urgent_event_queue :
            stop ? stop_event_queue : event_queue;
        queued = xQueueSend(queue, &message->event, 0);
        if (queued == pdTRUE) {
            counter_increment(&event_received);
        } else {
            counter_increment(urgent ? &urgent_event_overflow :
                stop ? &stop_event_overflow : &event_overflow);
        }
    } else if (message->type == SENSOR_LINK_RIDE) {
        queued = xQueueOverwrite(ride_queue, &message->ride);
        if (queued == pdTRUE) counter_increment(&ride_overwrites);
    } else if (message->type == SENSOR_LINK_ENVIRONMENT) {
        queued = xQueueOverwrite(environment_queue, &message->environment);
        if (queued == pdTRUE) counter_increment(&environment_overwrites);
    } else {
        counter_increment(&control_rejected);
    }

    if (queued == pdTRUE) {
        xTaskNotifyGive(worker);
    } else {
        ESP_LOGW(TAG, "LOCAL_ENVELOPE_QUEUE_FULL type=0x%02x", message->type);
    }
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
        /* STM is a local-envelope producer only. Reject legacy official UART
         * frames so sequence/session ownership cannot split again. */
        return result == NOSTOS_OK ? NOSTOS_UNAUTHORIZED : result;
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
    portEXIT_CRITICAL(&lock);
    if (source == own || !worker || own_source == 0U || !bound) return;
    nostos_result_t result = enqueue_remote(wire, length, source);
    ESP_LOGI(TAG, "MESH_RX address=0x%04x len=%u result=%s", source,
             (unsigned)length, nostos_result_name(result));
}

void bridge_runtime_mesh_complete(int error)
{
    nostos_result_t result;
    portENTER_CRITICAL(&lock);
    result = mesh_inflight_complete(&mesh_inflight, error);
    if (result == NOSTOS_OK) {
        if (error) ++async_failed; else ++async_ok;
    }
    portEXIT_CRITICAL(&lock);
    if (result == NOSTOS_OK) {
        if (worker != NULL) xTaskNotifyGive(worker);
    } else {
        ESP_LOGW(TAG, "MESH_COMPLETE_UNEXPECTED error=%d result=%s",
                 error, nostos_result_name(result));
    }
}

esp_err_t bridge_runtime_send_sensor_ride(bool valid, uint16_t kmh_x10,
                                          uint32_t distance_mm)
{
    if (!worker || !ride_queue) return ESP_ERR_INVALID_STATE;
    if (!valid && (kmh_x10 != 0U || distance_mm != 0U)) return ESP_ERR_INVALID_ARG;
    sensor_link_ride_t ride = {
        .valid = valid,
        .kmh_x10 = valid ? kmh_x10 : 0U,
        .distance_mm = valid ? distance_mm : 0U,
    };
    BaseType_t queued = xQueueOverwrite(ride_queue, &ride);
    if (queued != pdPASS) return ESP_FAIL;
    counter_increment(&ride_overwrites);
    xTaskNotifyGive(worker);
    return ESP_OK;
}

void bridge_runtime_log_status(void)
{
    portENTER_CRITICAL(&lock);
    identity_state_t id = identity;
    application_message_engine_stats_t engine_stats = app_engine.stats;
    uint32_t a = accepted, r = rejected, ok = async_ok, bad = async_failed;
    uint32_t publish_ok = local_publish_ok, publish_bad = local_publish_failed;
    uint32_t mirror_bad = local_mirror_failed;
    uint32_t ready_ok = ready_uart_ok, ready_bad = ready_uart_failed;
    uint32_t controls = control_received, controls_bad = control_rejected;
    uint32_t controls_full = control_overflow;
    uint32_t event_ok = event_received, event_full = event_overflow;
    uint32_t stop_full = stop_event_overflow;
    uint32_t urgent_full = urgent_event_overflow;
    uint32_t ride_latest = ride_overwrites;
    uint32_t environment_latest = environment_overwrites;
    uint32_t hits = cache_hits, misses = cache_misses;
    uint32_t cache_ok = cache_uart_ok, cache_bad = cache_uart_failed;
    size_t pending = id.bridge_initialized ? bridge.count : 0U;
    size_t pending_sensor_outputs = 0U;
    for (size_t i = 0U; i < SENSOR_OUTPUT_SLOT_COUNT; ++i) {
        if (cache_snapshots[i].pending) ++pending_sensor_outputs;
    }
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,
             "STATUS version=2 primary=0x%04x source=%u session=%" PRIu32
             " confirmed=%u waiting_ack=%u"
             " pending=%u accepted=%" PRIu32 " rejected=%" PRIu32
             " async_ok_total=%" PRIu32 " async_failed_total=%" PRIu32
             " local_publish_ok=%" PRIu32 " local_publish_failed=%" PRIu32
             " output_queue_failed=%" PRIu32 " ready_uart_ok=%" PRIu32
             " ready_uart_failed=%" PRIu32 " control_received=%" PRIu32
             " control_rejected=%" PRIu32 " control_overflow=%" PRIu32
             " event_received=%" PRIu32 " event_overflow=%" PRIu32
             " stop_event_overflow=%" PRIu32
             " urgent_event_overflow=%" PRIu32
             " ride_latest=%" PRIu32 " environment_latest=%" PRIu32
             " pending_sensor_outputs=%u cache_hits=%" PRIu32
             " cache_misses=%" PRIu32 " cache_uart_ok=%" PRIu32
             " cache_uart_failed=%" PRIu32
             " engine_accepted=%" PRIu32 " engine_duplicate=%" PRIu32
             " engine_rejected=%" PRIu32 " output_uart_ok=%" PRIu32
             " output_uart_failed=%" PRIu32
             " output_result_accepted=%" PRIu32
             " output_result_duplicate=%" PRIu32
             " output_result_rejected=%" PRIu32
             " output_result_hardware_error=%" PRIu32,
             id.primary_address, id.local_source, id.session_id, id.identity_confirmed,
             id.identity_waiting_ack, (unsigned)pending, a, r, ok, bad,
             publish_ok, publish_bad, mirror_bad, ready_ok, ready_bad,
             controls, controls_bad,
             controls_full, event_ok, event_full, stop_full, urgent_full, ride_latest,
             environment_latest, (unsigned)pending_sensor_outputs,
             hits, misses, cache_ok, cache_bad,
             engine_stats.accepted, engine_stats.duplicate,
             engine_stats.rejected, engine_stats.uart_tx_ok,
             engine_stats.uart_tx_failed,
             engine_stats.output_result_accepted,
             engine_stats.output_result_duplicate,
             engine_stats.output_result_rejected,
             engine_stats.output_result_hardware_error);
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

    mesh_retry_init(&mesh_retry);
    mesh_inflight_init(&mesh_inflight);
    application_event_heap_init(&event_outputs);
    output_command_retry_init(&output_retry);
    mesh_retry_next_ms = 0U;
    identity = (identity_state_t){0};
    official_writer = (official_packet_writer_t){0};
    app_engine = (application_message_engine_t){0};
    boot_session_committed = false;
    boot_session = 0U;
    reset_epoch_counters_locked();
    for (size_t i = 0U; i < SENSOR_OUTPUT_SLOT_COUNT; ++i) {
        cache_snapshots[i] = (cache_snapshot_slot_t){0};
    }
    err = uart_set_pin(DATA_UART, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err = uart_driver_install(DATA_UART, 512, 0, 16, &events, 0);
    if (err != ESP_OK) return err;

    ride_queue = xQueueCreateStatic(1U, sizeof(sensor_link_ride_t),
                                    ride_queue_storage, &ride_queue_control);
    environment_queue = xQueueCreateStatic(
        1U, sizeof(sensor_link_environment_t), environment_queue_storage,
        &environment_queue_control);
    event_queue = xQueueCreateStatic(
        EVENT_QUEUE_LENGTH, sizeof(sensor_link_event_t), event_queue_storage,
        &event_queue_control);
    stop_event_queue = xQueueCreateStatic(
        STOP_EVENT_QUEUE_LENGTH, sizeof(sensor_link_event_t),
        stop_event_queue_storage, &stop_event_queue_control);
    urgent_event_queue = xQueueCreateStatic(
        URGENT_EVENT_QUEUE_LENGTH, sizeof(sensor_link_event_t),
        urgent_event_queue_storage, &urgent_event_queue_control);
    control_queue = xQueueCreateStatic(CONTROL_QUEUE_LENGTH, sizeof(sensor_link_message_t),
                                       control_queue_storage, &control_queue_control);
    if (ride_queue == NULL || environment_queue == NULL ||
        event_queue == NULL || stop_event_queue == NULL ||
        urgent_event_queue == NULL ||
        control_queue == NULL) {
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
