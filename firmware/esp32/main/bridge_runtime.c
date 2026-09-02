/* One volatile application runtime. Mesh identity is the only source of a
 * node id; reboot deliberately clears state, request dedupe and STOP ACKs. */
#include "bridge_runtime.h"

#include "application_runtime.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mesh_node.h"
#include "nostos_protocol.h"
#include "nostos_uart.h"

#include <inttypes.h>

#define TAG "NOSTOS"
#define DATA_UART UART_NUM_1
#define NORMAL_QUEUE_LENGTH 16U
#define STOP_REQUEST_QUEUE_LENGTH 10U
#define STOP_ACK_QUEUE_LENGTH 10U
#define WORKER_POLL_MS 50U

typedef enum {
    RUNTIME_EVENT_LOCAL_UART = 0,
    RUNTIME_EVENT_MESH,
    RUNTIME_EVENT_XOSS_RIDE,
} runtime_event_kind_t;

typedef struct {
    runtime_event_kind_t kind;
    uint16_t mesh_source_address;
    nostos_message_t message;
    struct {
        bool sensor_valid;
        uint16_t speed_x10_kmh;
        uint32_t trip_distance_m;
    } ride;
} runtime_event_t;

typedef struct {
    uint32_t mesh_rx_ok;
    uint32_t mesh_rx_rejected;
    uint32_t mesh_tx_api_accepted;
    uint32_t mesh_tx_api_failed;
    uint32_t mesh_tx_complete_ok;
    uint32_t mesh_tx_complete_failed;
    uint32_t uart_rx_ok;
    uint32_t uart_rx_rejected;
    uint32_t uart_tx_ok;
    uint32_t uart_tx_failed;
    uint32_t local_stop_ack_tx_failed;
    uint32_t stop_duplicates;
    uint32_t normal_queue_overflow;
    uint32_t stop_request_queue_overflow;
    uint32_t stop_ack_queue_overflow;
    uint32_t state_slot_replaced;
    uint32_t queue_dropped_unbound;
    uint32_t state_fresh;
    uint32_t state_stale;
    uint32_t state_unknown;
    uint32_t sensor_invalid;
    uint16_t stop_pending_mask;
} runtime_stats_t;

static const struct {
    uint16_t mesh_address;
    uint8_t source_node_id;
} peers[] = {
    {CONFIG_NOSTOS_SOURCE1_ADDRESS, 1U},
    {CONFIG_NOSTOS_SOURCE2_ADDRESS, 2U},
    {CONFIG_NOSTOS_SOURCE3_ADDRESS, 3U},
    {CONFIG_NOSTOS_SOURCE4_ADDRESS, 4U},
    {CONFIG_NOSTOS_SOURCE5_ADDRESS, 5U},
    {CONFIG_NOSTOS_SOURCE6_ADDRESS, 6U},
    {CONFIG_NOSTOS_SOURCE7_ADDRESS, 7U},
    {CONFIG_NOSTOS_SOURCE8_ADDRESS, 8U},
    {CONFIG_NOSTOS_SOURCE9_ADDRESS, 9U},
    {CONFIG_NOSTOS_SOURCE10_ADDRESS, 10U},
};

static application_runtime_t runtime;
static bool runtime_initialized;
static bool runtime_ready;
static uint8_t local_source_node_id;
static uint16_t local_primary_address;
static runtime_stats_t stats;
static bool configuration_error_logged;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static QueueHandle_t normal_queue;
static QueueHandle_t stop_request_queue;
static QueueHandle_t stop_ack_queue;
static QueueHandle_t uart_events;
static StaticQueue_t normal_queue_control;
static StaticQueue_t stop_request_queue_control;
static StaticQueue_t stop_ack_queue_control;
static uint8_t normal_queue_storage[
    NORMAL_QUEUE_LENGTH * sizeof(runtime_event_t)];
static uint8_t stop_request_queue_storage[
    STOP_REQUEST_QUEUE_LENGTH * sizeof(runtime_event_t)];
static uint8_t stop_ack_queue_storage[
    STOP_ACK_QUEUE_LENGTH * sizeof(runtime_event_t)];
static runtime_event_t latest_state_events[2];
static bool latest_state_valid[2];

static uint32_t now_ms(void)
{
    return (uint32_t)((uint64_t)esp_timer_get_time() / 1000U);
}

static uint8_t source_for_address(uint16_t address)
{
    if (address == 0U) return 0U;
    for (size_t i = 0U; i < sizeof(peers) / sizeof(peers[0]); ++i) {
        if (peers[i].mesh_address == address) return peers[i].source_node_id;
    }
    return 0U;
}

static uint16_t address_for_source(uint8_t source_node_id)
{
    for (size_t i = 0U; i < sizeof(peers) / sizeof(peers[0]); ++i) {
        if (peers[i].source_node_id == source_node_id) {
            return peers[i].mesh_address;
        }
    }
    return 0U;
}

static uint16_t configured_peer_mask(void)
{
    uint16_t mask = 0U;
    for (size_t i = 0U; i < sizeof(peers) / sizeof(peers[0]); ++i) {
        if (peers[i].mesh_address != 0U && peers[i].source_node_id >= 1U &&
            peers[i].source_node_id <= APPLICATION_RUNTIME_MAX_NODES) {
            mask |= (uint16_t)(1U << (peers[i].source_node_id - 1U));
        }
    }
    return mask;
}

static bool peer_configuration_valid(void)
{
    for (size_t i = 0U; i < sizeof(peers) / sizeof(peers[0]); ++i) {
        if (peers[i].mesh_address == 0U) continue;
        for (size_t j = i + 1U; j < sizeof(peers) / sizeof(peers[0]); ++j) {
            if (peers[i].mesh_address == peers[j].mesh_address) return false;
        }
    }
    return address_for_source(CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE) != 0U &&
        address_for_source(CONFIG_NOSTOS_ENVIRONMENT_PUBLISHER_SOURCE) != 0U;
}

static uint32_t random_request_id(void)
{
    uint32_t request_id = 0U;
    while (request_id == 0U) request_id = esp_random();
    return request_id;
}

static void increment(uint32_t *counter)
{
    portENTER_CRITICAL(&lock);
    ++*counter;
    portEXIT_CRITICAL(&lock);
}

static uint8_t event_message_type(const runtime_event_t *event)
{
    if (event == NULL) return 0U;
    if (event->kind == RUNTIME_EVENT_LOCAL_UART ||
        event->kind == RUNTIME_EVENT_MESH) {
        return event->message.type;
    }
    return 0U;
}

static int event_state_slot(
    const runtime_event_t *event,
    uint8_t local_source)
{
    if (event == NULL) return -1;
    if (event->kind == RUNTIME_EVENT_XOSS_RIDE) {
        return local_source == CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE ? 0 : -1;
    }
    if (event->kind == RUNTIME_EVENT_LOCAL_UART &&
        event->message.type == NOSTOS_MESSAGE_STATE_UPDATE &&
        event->message.payload.state_update.topic_id ==
            NOSTOS_TOPIC_ENVIRONMENT) {
        return local_source == CONFIG_NOSTOS_ENVIRONMENT_PUBLISHER_SOURCE
            ? 1 : -1;
    }
    if (event->kind != RUNTIME_EVENT_MESH ||
        event->message.type != NOSTOS_MESSAGE_STATE_UPDATE) {
        return -1;
    }
    uint8_t mesh_source = source_for_address(event->mesh_source_address);
    if (event->message.payload.state_update.topic_id == NOSTOS_TOPIC_RIDE &&
        mesh_source == CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE) {
        return 0;
    }
    if (event->message.payload.state_update.topic_id ==
            NOSTOS_TOPIC_ENVIRONMENT &&
        mesh_source == CONFIG_NOSTOS_ENVIRONMENT_PUBLISHER_SOURCE) {
        return 1;
    }
    return -1;
}

static bool enqueue(const runtime_event_t *event)
{
    portENTER_CRITICAL(&lock);
    bool bound = runtime_ready;
    uint8_t source = local_source_node_id;
    if (!bound) ++stats.queue_dropped_unbound;
    portEXIT_CRITICAL(&lock);
    if (!bound) return false;
    if (event == NULL) return false;

    int state_slot = event_state_slot(event, source);
    if (state_slot >= 0) {
        portENTER_CRITICAL(&lock);
        if (latest_state_valid[state_slot]) ++stats.state_slot_replaced;
        latest_state_events[state_slot] = *event;
        latest_state_valid[state_slot] = true;
        portEXIT_CRITICAL(&lock);
        if (worker != NULL) xTaskNotifyGive(worker);
        return true;
    }

    QueueHandle_t queue = normal_queue;
    uint32_t *overflow = &stats.normal_queue_overflow;
    uint8_t message_type = event_message_type(event);
    if (message_type == NOSTOS_MESSAGE_STOP_REQUEST) {
        queue = stop_request_queue;
        overflow = &stats.stop_request_queue_overflow;
    } else if (message_type == NOSTOS_MESSAGE_STOP_ACK) {
        queue = stop_ack_queue;
        overflow = &stats.stop_ack_queue_overflow;
    }
    if (queue == NULL || xQueueSend(queue, event, 0U) != pdTRUE) {
        increment(overflow);
        return false;
    }
    if (worker != NULL) xTaskNotifyGive(worker);
    return true;
}

static void mark_runtime_unavailable(bool clear_identity)
{
    portENTER_CRITICAL(&lock);
    runtime_ready = false;
    if (clear_identity) {
        runtime_initialized = false;
        local_source_node_id = 0U;
        local_primary_address = 0U;
        stats.stop_pending_mask = 0U;
        stats.state_fresh = 0U;
        stats.state_stale = 0U;
        stats.state_unknown = 0U;
        stats.sensor_invalid = 0U;
    }
    portEXIT_CRITICAL(&lock);
}

static bool bind_runtime(void)
{
    if (!peer_configuration_valid()) {
        mark_runtime_unavailable(true);
        if (!configuration_error_logged) {
            configuration_error_logged = true;
            ESP_LOGE(TAG,
                     "PEER_CONFIG_INVALID addresses must be unique;"
                     " active publisher addresses must be nonzero");
        }
        return false;
    }
    uint16_t primary = mesh_node_primary();
    uint8_t source = source_for_address(primary);
    if (primary == 0U || source == 0U) {
        /* Provisioning identity disappeared or is not one of our configured
         * nodes. Old STOP/state RAM must not cross that identity boundary. */
        mark_runtime_unavailable(true);
        return false;
    }
    if (!mesh_node_ready()) {
        /* Keep an already-bound runtime while model configuration is
         * temporarily incomplete. New ingress remains rejected, but a STOP
         * already ACKed to STM32 resumes retrying after the same identity is
         * ready again. */
        mark_runtime_unavailable(false);
        return false;
    }
    if (runtime_initialized && source == local_source_node_id &&
        primary == local_primary_address) {
        bool resumed;
        portENTER_CRITICAL(&lock);
        resumed = !runtime_ready;
        runtime_ready = true;
        portEXIT_CRITICAL(&lock);
        if (resumed) {
            ESP_LOGI(TAG,
                     "IDENTITY_RESUMED primary=0x%04x source=%u"
                     " stop_pending=0x%03x",
                     primary, source,
                     application_runtime_stop_pending_mask(&runtime));
        }
        return true;
    }

    nostos_result_t result = application_runtime_init(
        &runtime, source, configured_peer_mask(),
        CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE,
        CONFIG_NOSTOS_ENVIRONMENT_PUBLISHER_SOURCE);
    if (result != NOSTOS_OK) {
        ESP_LOGE(TAG, "RUNTIME_INIT_FAILED source=%u result=%s",
                 source, nostos_result_name(result));
        mark_runtime_unavailable(true);
        return false;
    }
    portENTER_CRITICAL(&lock);
    local_source_node_id = source;
    local_primary_address = primary;
    runtime_initialized = true;
    runtime_ready = true;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG, "IDENTITY_BOUND primary=0x%04x source=%u volatile=1",
             primary, source);
    return true;
}

static bool uart_write_message(const nostos_message_t *message)
{
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t length = 0U;
    if (nostos_uart_encode_message(
            message, frame, sizeof(frame), &length) != NOSTOS_OK) {
        increment(&stats.uart_tx_failed);
        return false;
    }
    size_t sent = 0U;
    uint32_t started = now_ms();
    while (sent < length) {
        if ((uint32_t)(now_ms() - started) > 20U) {
            increment(&stats.uart_tx_failed);
            return false;
        }
        int count = uart_tx_chars(
            DATA_UART, (const char *)&frame[sent], length - sent);
        if (count < 0) {
            increment(&stats.uart_tx_failed);
            return false;
        }
        sent += (size_t)count;
        if (sent < length) vTaskDelay(1U);
    }
    bool ok = uart_wait_tx_done(DATA_UART, pdMS_TO_TICKS(20U)) == ESP_OK;
    increment(ok ? &stats.uart_tx_ok : &stats.uart_tx_failed);
    return ok;
}

static bool mesh_send(uint16_t destination, const nostos_message_t *message)
{
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    size_t length = 0U;
    nostos_result_t encoded = nostos_message_encode(
        message, wire, sizeof(wire), &length);
    esp_err_t sent = encoded == NOSTOS_OK
        ? mesh_node_send_to(destination, wire, length)
        : ESP_ERR_INVALID_ARG;
    increment(sent == ESP_OK ?
        &stats.mesh_tx_api_accepted : &stats.mesh_tx_api_failed);
    if (sent != ESP_OK) {
        ESP_LOGW(TAG, "MESH_TX_FAILED dst=0x%04x type=%s err=%s",
                 destination, nostos_message_type_name(message->type),
                 esp_err_to_name(sent));
    }
    return sent == ESP_OK;
}

static void handle_local_message(nostos_message_t message)
{
    if (!runtime_ready) {
        increment(&stats.uart_rx_rejected);
        return;
    }

    if (message.type == NOSTOS_MESSAGE_STOP_ACK) {
        uint8_t remote_source = 0U;
        nostos_result_t completed =
            application_runtime_complete_remote_stop(
                &runtime, &message, &remote_source);
        uint16_t destination = address_for_source(remote_source);
        if (completed != NOSTOS_OK || destination == 0U) {
            increment(&stats.uart_rx_rejected);
            return;
        }
        nostos_message_t mesh_ack;
        if (nostos_message_make_stop_ack(
                &mesh_ack, local_source_node_id,
                message.payload.stop_ack.request_id) != NOSTOS_OK) {
            increment(&stats.uart_rx_rejected);
            return;
        }
        increment(&stats.uart_rx_ok);
        (void)mesh_send(destination, &mesh_ack);
        return;
    }

    if (message.type == NOSTOS_MESSAGE_STOP_REQUEST) {
        const uint32_t local_request_id =
            message.payload.stop_request.request_id;
        application_local_stop_action_t action;
        nostos_message_t mesh_request;
        nostos_result_t accepted = application_runtime_accept_local_stop(
            &runtime, &message, random_request_id(), now_ms(),
            &action, &mesh_request);
        if (accepted != NOSTOS_OK) {
            increment(&stats.uart_rx_rejected);
            return;
        }

        /* Runtime ownership and peer retry state are committed before the
         * paired STM32 is told that ESP32 accepted this local transaction. */
        nostos_message_t local_ack;
        bool ack_ok = nostos_message_make_stop_ack(
                &local_ack, local_source_node_id, local_request_id) == NOSTOS_OK &&
            uart_write_message(&local_ack);
        if (!ack_ok) increment(&stats.local_stop_ack_tx_failed);

        increment(&stats.uart_rx_ok);
        if (action == APPLICATION_LOCAL_STOP_ACK_ONLY) {
            increment(&stats.stop_duplicates);
            return;
        }
        /* One group send first. Later retries use the missing-peer mask only. */
        (void)mesh_send(BSG_EVENT_GROUP, &mesh_request);
        return;
    }

    if (application_runtime_stamp_local(&runtime, &message) != NOSTOS_OK) {
        increment(&stats.uart_rx_rejected);
        return;
    }
    /* source 0 existed only on the paired UART seam. Normal Mesh encoding
     * below validates the stamped, provisioned 1..10 identity. */

    if (message.type == NOSTOS_MESSAGE_STATE_UPDATE) {
        /* The paired DHT path owns ENVIRONMENT. RIDE is owned by XOSS below. */
        if (message.payload.state_update.topic_id != NOSTOS_TOPIC_ENVIRONMENT ||
            application_runtime_publish_state(
                &runtime, &message, now_ms()) != NOSTOS_OK) {
            increment(&stats.uart_rx_rejected);
            return;
        }
        /* Mesh does not loop the publisher's own group message back. Mirror
         * the stamped state so this node's dashboard sees the shared feed. */
        (void)uart_write_message(&message);
        increment(&stats.uart_rx_ok);
        return;
    }
    if (message.type == NOSTOS_MESSAGE_PACE_REQUEST) {
        message.payload.pace_request.request_id = random_request_id();
        increment(&stats.uart_rx_ok);
        (void)mesh_send(BSG_EVENT_GROUP, &message);
        return;
    }
    increment(&stats.uart_rx_rejected);
}

static void handle_mesh_message(const runtime_event_t *event)
{
    if (!runtime_ready || event == NULL) return;
    uint8_t bound_source = source_for_address(event->mesh_source_address);
    nostos_message_t message = event->message;
    if (bound_source == 0U || message.source_node_id != bound_source) {
        increment(&stats.mesh_rx_rejected);
        ESP_LOGW(TAG, "MESH_RX_REJECT address=0x%04x payload_source=%u",
                 event->mesh_source_address,
                 message.source_node_id);
        return;
    }

    if (message.type == NOSTOS_MESSAGE_STATE_UPDATE) {
        if (application_runtime_accept_mesh_state(
                &runtime, &message, bound_source, now_ms()) == NOSTOS_OK) {
            increment(&stats.mesh_rx_ok);
            (void)uart_write_message(&message);
        } else {
            increment(&stats.mesh_rx_rejected);
        }
        return;
    }
    if (message.type == NOSTOS_MESSAGE_PACE_REQUEST) {
        bool duplicate = false;
        nostos_result_t accepted = application_runtime_request_is_duplicate(
            &runtime, &message, bound_source, &duplicate);
        if (accepted == NOSTOS_OK) {
            increment(&stats.mesh_rx_ok);
            if (!duplicate) {
                (void)application_runtime_remember_request(
                    &runtime, &message, bound_source);
                (void)uart_write_message(&message);
            }
        } else {
            increment(&stats.mesh_rx_rejected);
        }
        return;
    }
    if (message.type == NOSTOS_MESSAGE_STOP_REQUEST) {
        application_remote_stop_action_t action;
        nostos_result_t accepted = application_runtime_accept_remote_stop(
            &runtime, &message, bound_source, &action);
        if (accepted != NOSTOS_OK) {
            increment(&stats.mesh_rx_rejected);
            return;
        }
        if (action == APPLICATION_REMOTE_STOP_FORWARD_UART) {
            if (!uart_write_message(&message)) {
                /* No Mesh ACK: the sender retries, and pending duplicates are
                 * deliberately forwarded to STM32 again for ACK recovery. */
                increment(&stats.mesh_rx_rejected);
                return;
            }
            increment(&stats.mesh_rx_ok);
            return;
        }
        nostos_message_t ack;
        if (nostos_message_make_stop_ack(
                &ack, local_source_node_id,
                message.payload.stop_request.request_id) == NOSTOS_OK) {
            (void)mesh_send(event->mesh_source_address, &ack);
        }
        increment(&stats.mesh_rx_ok);
        increment(&stats.stop_duplicates);
        return;
    }
    if (message.type == NOSTOS_MESSAGE_STOP_ACK) {
        nostos_result_t accepted = application_runtime_accept_stop_ack(
            &runtime, &message, bound_source);
        increment(accepted == NOSTOS_OK ?
            &stats.mesh_rx_ok : &stats.mesh_rx_rejected);
        return;
    }
    increment(&stats.mesh_rx_rejected);
}

static void handle_xoss_ride(const runtime_event_t *event)
{
    if (!runtime_ready || event == NULL) return;
    if (local_source_node_id != CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE) {
        ESP_LOGW(TAG, "XOSS_STATE_REJECT source=%u configured=%u",
                 local_source_node_id,
                 CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE);
        return;
    }
    nostos_message_t message;
    nostos_result_t made = nostos_message_make_ride(
        &message, local_source_node_id, event->ride.sensor_valid,
        event->ride.speed_x10_kmh, event->ride.trip_distance_m);
    if (made != NOSTOS_OK || application_runtime_publish_state(
            &runtime, &message, now_ms()) != NOSTOS_OK) {
        ESP_LOGW(TAG, "XOSS_STATE_REJECT result=%s",
                 nostos_result_name(made));
    } else {
        /* Mesh does not loop the publisher's own group message back. */
        (void)uart_write_message(&message);
    }
}

static void publish_due_state(uint32_t now)
{
    nostos_message_t message;
    while (application_runtime_take_due_state(
               &runtime, now, &message) == NOSTOS_OK) {
        (void)mesh_send(BSG_EVENT_GROUP, &message);
    }
}

static void retry_missing_stop_peers(uint32_t now)
{
    uint16_t missing = 0U;
    nostos_message_t request;
    if (application_runtime_stop_retry_due(
            &runtime, now, &missing, &request) != NOSTOS_OK) {
        return;
    }
    for (uint8_t source = 1U;
         source <= APPLICATION_RUNTIME_MAX_NODES; ++source) {
        if ((missing & (uint16_t)(1U << (source - 1U))) == 0U) continue;
        uint16_t destination = address_for_source(source);
        if (destination != 0U) (void)mesh_send(destination, &request);
    }
}

static void update_runtime_status(uint32_t now)
{
    const nostos_topic_t topics[] = {
        NOSTOS_TOPIC_RIDE, NOSTOS_TOPIC_ENVIRONMENT,
    };
    const uint8_t publishers[] = {
        CONFIG_NOSTOS_RIDE_PUBLISHER_SOURCE,
        CONFIG_NOSTOS_ENVIRONMENT_PUBLISHER_SOURCE,
    };
    uint32_t fresh = 0U, stale = 0U, unknown = 0U, invalid = 0U;
    for (size_t topic = 0U; topic < 2U; ++topic) {
        bool sensor_valid = false;
        application_state_freshness_t freshness = application_runtime_state(
            &runtime, topics[topic], publishers[topic], now,
            NULL, &sensor_valid);
        if (freshness == APPLICATION_STATE_FRESH) ++fresh;
        else if (freshness == APPLICATION_STATE_STALE) ++stale;
        else ++unknown;
        if (freshness != APPLICATION_STATE_UNKNOWN && !sensor_valid) ++invalid;
    }
    uint16_t pending = application_runtime_stop_pending_mask(&runtime);
    portENTER_CRITICAL(&lock);
    stats.state_fresh = fresh;
    stats.state_stale = stale;
    stats.state_unknown = unknown;
    stats.sensor_invalid = invalid;
    stats.stop_pending_mask = pending;
    portEXIT_CRITICAL(&lock);
}

static void drop_queued_unbound(void)
{
    runtime_event_t event;
    uint32_t dropped = 0U;
    QueueHandle_t queues[] = {
        stop_request_queue, stop_ack_queue, normal_queue,
    };
    for (size_t i = 0U; i < sizeof(queues) / sizeof(queues[0]); ++i) {
        while (queues[i] != NULL &&
               xQueueReceive(queues[i], &event, 0U) == pdTRUE) {
            ++dropped;
        }
    }
    portENTER_CRITICAL(&lock);
    for (size_t i = 0U; i < 2U; ++i) {
        if (latest_state_valid[i]) ++dropped;
        latest_state_valid[i] = false;
    }
    portEXIT_CRITICAL(&lock);
    if (dropped != 0U) {
        portENTER_CRITICAL(&lock);
        stats.queue_dropped_unbound += dropped;
        portEXIT_CRITICAL(&lock);
    }
}

static void handle_event(const runtime_event_t *event);

static bool take_latest_state(size_t slot, runtime_event_t *event)
{
    bool available = false;
    portENTER_CRITICAL(&lock);
    if (slot < 2U && latest_state_valid[slot]) {
        *event = latest_state_events[slot];
        latest_state_valid[slot] = false;
        available = true;
    }
    portEXIT_CRITICAL(&lock);
    return available;
}

static void drain_latest_states(void)
{
    runtime_event_t event;
    for (size_t slot = 0U; slot < 2U; ++slot) {
        if (take_latest_state(slot, &event)) handle_event(&event);
    }
}

static void handle_event(const runtime_event_t *event)
{
    if (event->kind == RUNTIME_EVENT_LOCAL_UART) {
        handle_local_message(event->message);
    } else if (event->kind == RUNTIME_EVENT_MESH) {
        handle_mesh_message(event);
    } else if (event->kind == RUNTIME_EVENT_XOSS_RIDE) {
        handle_xoss_ride(event);
    }
}

static void drain_queue(QueueHandle_t queue)
{
    runtime_event_t event;
    UBaseType_t budget = queue != NULL ? uxQueueMessagesWaiting(queue) : 0U;
    while (budget-- > 0U && xQueueReceive(queue, &event, 0U) == pdTRUE) {
        handle_event(&event);
    }
}

static void worker_task(void *argument)
{
    (void)argument;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WORKER_POLL_MS));
        if (!bind_runtime()) {
            drop_queued_unbound();
            continue;
        }
        /* Control traffic has independent capacity. Snapshot-sized drains keep
         * request priority without allowing a continuously refilled queue to
         * starve ACKs or telemetry forever. */
        drain_queue(stop_request_queue);
        drain_queue(stop_ack_queue);
        drain_latest_states();
        drain_queue(normal_queue);
        uint32_t now = now_ms();
        publish_due_state(now);
        retry_missing_stop_peers(now);
        update_runtime_status(now);
    }
}

static void uart_task(void *argument)
{
    (void)argument;
    nostos_uart_parser_t parser = {0};
    nostos_uart_reset(&parser);
    uart_event_t uart_event;
    for (;;) {
        if (xQueueReceive(uart_events, &uart_event, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (uart_event.type == UART_DATA) {
            for (size_t i = 0U; i < uart_event.size; ++i) {
                uint8_t byte = 0U;
                if (uart_read_bytes(DATA_UART, &byte, 1U, 0U) != 1) break;
                nostos_message_t message;
                nostos_result_t result = nostos_uart_feed_local_message(
                    &parser, byte, now_ms(), &message);
                if (result == NOSTOS_OK) {
                    runtime_event_t event = {
                        .kind = RUNTIME_EVENT_LOCAL_UART,
                        .message = message,
                    };
                    (void)enqueue(&event);
                } else if (result != NOSTOS_EMPTY) {
                    increment(&stats.uart_rx_rejected);
                    ESP_LOGW(TAG, "UART_RX_REJECT result=%s",
                             nostos_result_name(result));
                }
            }
        } else if (uart_event.type == UART_FIFO_OVF ||
                   uart_event.type == UART_BUFFER_FULL ||
                   uart_event.type == UART_PARITY_ERR ||
                   uart_event.type == UART_FRAME_ERR) {
            nostos_uart_reset(&parser);
            uart_flush_input(DATA_UART);
            xQueueReset(uart_events);
            increment(&stats.uart_rx_rejected);
        }
    }
}

void bridge_runtime_mesh_rx(
    const uint8_t *wire,
    size_t length,
    uint16_t source,
    uint16_t own_address)
{
    if (source == own_address) return;
    uint8_t bound_source = source_for_address(source);
    if (wire == NULL || length > NOSTOS_APPLICATION_MAX_SIZE ||
        bound_source == 0U || source_for_address(own_address) == 0U) {
        increment(&stats.mesh_rx_rejected);
        return;
    }
    nostos_message_t message;
    if (nostos_message_decode(wire, length, &message) != NOSTOS_OK ||
        message.source_node_id != bound_source) {
        increment(&stats.mesh_rx_rejected);
        return;
    }
    runtime_event_t event = {
        .kind = RUNTIME_EVENT_MESH,
        .mesh_source_address = source,
        .message = message,
    };
    (void)enqueue(&event);
}

void bridge_runtime_mesh_complete(int error)
{
    increment(error == 0 ?
        &stats.mesh_tx_complete_ok : &stats.mesh_tx_complete_failed);
    if (error != 0) {
        ESP_LOGW(TAG, "MESH_SEND_COMPLETE error=%d", error);
    }
}

esp_err_t bridge_runtime_send_sensor_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm)
{
    if (worker == NULL) return ESP_ERR_INVALID_STATE;
    if (!valid && (kmh_x10 != 0U || distance_mm != 0U)) {
        return ESP_ERR_INVALID_ARG;
    }
    runtime_event_t event = {
        .kind = RUNTIME_EVENT_XOSS_RIDE,
        .ride = {
            .sensor_valid = valid,
            .speed_x10_kmh = valid ? kmh_x10 : 0U,
            .trip_distance_m = valid ? distance_mm / 1000U : 0U,
        },
    };
    return enqueue(&event) ? ESP_OK : ESP_FAIL;
}

uint8_t bridge_runtime_local_source_node_id(void)
{
    portENTER_CRITICAL(&lock);
    uint8_t source = runtime_ready ? local_source_node_id : 0U;
    portEXIT_CRITICAL(&lock);
    return source;
}

void bridge_runtime_log_status(void)
{
    runtime_stats_t snapshot;
    uint8_t source;
    uint16_t primary;
    bool initialized;
    bool ready;
    portENTER_CRITICAL(&lock);
    snapshot = stats;
    initialized = runtime_initialized;
    ready = runtime_ready;
    source = initialized ? local_source_node_id : 0U;
    primary = initialized ? local_primary_address : 0U;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,
             "STATUS source=%u primary=0x%04x initialized=%u ready=%u"
             " volatile=1 max_nodes=%u"
             " mesh_rx_ok=%" PRIu32 " mesh_rx_rejected=%" PRIu32
             " mesh_tx_api_accepted=%" PRIu32
             " mesh_tx_api_failed=%" PRIu32
             " mesh_tx_complete_ok=%" PRIu32
             " mesh_tx_complete_failed=%" PRIu32
             " uart_rx_ok=%" PRIu32 " uart_rx_rejected=%" PRIu32
             " uart_tx_ok=%" PRIu32 " uart_tx_failed=%" PRIu32
             " local_stop_ack_tx_failed=%" PRIu32
             " stop_pending=0x%03x stop_duplicates=%" PRIu32
             " state_fresh=%" PRIu32 " state_stale=%" PRIu32
             " state_unknown=%" PRIu32 " sensor_invalid=%" PRIu32
             " normal_queue_overflow=%" PRIu32
             " stop_request_queue_overflow=%" PRIu32
             " stop_ack_queue_overflow=%" PRIu32
             " state_slot_replaced=%" PRIu32
             " queue_dropped_unbound=%" PRIu32,
             source, primary, initialized, ready,
             APPLICATION_RUNTIME_MAX_NODES,
             snapshot.mesh_rx_ok, snapshot.mesh_rx_rejected,
             snapshot.mesh_tx_api_accepted, snapshot.mesh_tx_api_failed,
             snapshot.mesh_tx_complete_ok,
             snapshot.mesh_tx_complete_failed,
             snapshot.uart_rx_ok, snapshot.uart_rx_rejected,
             snapshot.uart_tx_ok, snapshot.uart_tx_failed,
             snapshot.local_stop_ack_tx_failed,
             snapshot.stop_pending_mask,
             snapshot.stop_duplicates,
             snapshot.state_fresh, snapshot.state_stale,
             snapshot.state_unknown, snapshot.sensor_invalid,
             snapshot.normal_queue_overflow,
             snapshot.stop_request_queue_overflow,
             snapshot.stop_ack_queue_overflow,
             snapshot.state_slot_replaced,
             snapshot.queue_dropped_unbound);
}

esp_err_t bridge_runtime_init(void)
{
    const uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t result = uart_param_config(DATA_UART, &config);
    if (result != ESP_OK) return result;
    result = uart_set_pin(
        DATA_UART, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (result != ESP_OK) return result;
    result = uart_driver_install(
        DATA_UART, 512U, 0U, 16U, &uart_events, 0U);
    if (result != ESP_OK) return result;

    runtime = (application_runtime_t){0};
    stats = (runtime_stats_t){0};
    runtime_initialized = false;
    runtime_ready = false;
    configuration_error_logged = false;
    local_source_node_id = 0U;
    local_primary_address = 0U;
    latest_state_valid[0] = false;
    latest_state_valid[1] = false;
    normal_queue = xQueueCreateStatic(
        NORMAL_QUEUE_LENGTH, sizeof(runtime_event_t),
        normal_queue_storage, &normal_queue_control);
    stop_request_queue = xQueueCreateStatic(
        STOP_REQUEST_QUEUE_LENGTH, sizeof(runtime_event_t),
        stop_request_queue_storage, &stop_request_queue_control);
    stop_ack_queue = xQueueCreateStatic(
        STOP_ACK_QUEUE_LENGTH, sizeof(runtime_event_t),
        stop_ack_queue_storage, &stop_ack_queue_control);
    if (normal_queue == NULL || stop_request_queue == NULL ||
        stop_ack_queue == NULL) {
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(
            worker_task, "nostos_worker", 5120U, NULL, 5U,
            &worker) != pdPASS) {
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(
            uart_task, "nostos_uart", 4096U, NULL, 4U,
            NULL) != pdPASS) {
        vTaskDelete(worker);
        worker = NULL;
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,
             "UART1_READY TX=17 RX=18 framing=A5-5A source0=local-only");
    return ESP_OK;
}
