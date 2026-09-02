#include "message_protocol_service.h"

#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "display_service.h"
#include "message_type.h"
#include "nostos_uart.h"
#include "sensor_view_service.h"

#include <limits.h>
#include <stddef.h>

#define RX_CAPACITY 512U
#define RX_PROCESS_BUDGET 64U
#define UART_TX_TIMEOUT_MS 20U
#define STOP_RETRY_MS 200U
#define RECENT_REQUEST_CAPACITY 16U
#define LOCAL_PACE_REQUEST_ID_PLACEHOLDER 1U
#define LOCAL_STOP_REQUEST_ID_INITIAL 1U

/* The hardware monitor reads this static struct from the ELF. Fail the build
 * instead of silently changing that observable ABI. */
_Static_assert(sizeof(message_protocol_stats_t) == 52U,
    "message_protocol_stats_t monitor ABI size changed");
_Static_assert(offsetof(message_protocol_stats_t, received) == 0U,
    "message protocol received offset changed");
_Static_assert(offsetof(message_protocol_stats_t, rejected) == 4U,
    "message protocol rejected offset changed");
_Static_assert(offsetof(message_protocol_stats_t, duplicates) == 8U,
    "message protocol duplicates offset changed");
_Static_assert(offsetof(message_protocol_stats_t, stop_requests) == 24U,
    "message protocol STOP request offset changed");
_Static_assert(offsetof(message_protocol_stats_t, stop_acks) == 28U,
    "message protocol STOP ACK offset changed");
_Static_assert(offsetof(message_protocol_stats_t, stop_ack_matches) == 32U,
    "message protocol STOP ACK match offset changed");
_Static_assert(offsetof(message_protocol_stats_t, stop_ack_ignored) == 36U,
    "message protocol STOP ACK ignored offset changed");
_Static_assert(offsetof(message_protocol_stats_t, transmitted) == 40U,
    "message protocol transmitted offset changed");
_Static_assert(offsetof(message_protocol_stats_t, transmit_failures) == 44U,
    "message protocol transmit failure offset changed");
_Static_assert(offsetof(message_protocol_stats_t, last_protocol_result) == 48U,
    "message protocol result offset changed");

typedef struct {
    uint32_t request_id;
    uint8_t source_node_id;
    uint8_t type;
} recent_request_t;

typedef struct {
    bool active;
    nostos_message_t request;
    uint32_t retry_at_ms;
} pending_stop_t;

static UART_HandleTypeDef *data_uart;
static vs1003b_status_t audio_status;
static bool booted;
static bool fall_failsafe_active;
static pending_stop_t pending_stop;
static uint32_t next_local_request_id;
static nostos_uart_parser_t parser;
static message_protocol_stats_t stats;

static volatile uint8_t rx_bytes[RX_CAPACITY];
static volatile uint32_t rx_times[RX_CAPACITY];
static volatile unsigned rx_head;
static volatile unsigned rx_tail;
static volatile bool rx_overflow;

static recent_request_t recent_requests[RECENT_REQUEST_CAPACITY];
static size_t recent_request_next;

static bool due(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint32_t take_local_request_id(void)
{
    uint32_t request_id = next_local_request_id;
    ++next_local_request_id;
    if (next_local_request_id == 0U) {
        next_local_request_id = LOCAL_STOP_REQUEST_ID_INITIAL;
    }
    return request_id;
}

static void clear_pending_stop(void)
{
    pending_stop = (pending_stop_t){0};
}

static void reset_rx(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow = false;
    __set_PRIMASK(mask);
    nostos_uart_reset(&parser);
}

static message_protocol_result_t transmit_message(
    const nostos_message_t *message)
{
    if (!booted || data_uart == NULL) return MESSAGE_PROTOCOL_NOT_READY;
    if (message == NULL) return MESSAGE_PROTOCOL_BAD_ARGUMENT;

    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    nostos_result_t result = nostos_uart_encode_local_message(
        message, frame, sizeof(frame), &frame_length);
    stats.last_protocol_result = result;
    if (result == NOSTOS_BAD_ARGUMENT) return MESSAGE_PROTOCOL_BAD_ARGUMENT;
    if (result != NOSTOS_OK) return MESSAGE_PROTOCOL_BAD_VALUE;

    if (HAL_UART_Transmit(data_uart, frame, (uint16_t)frame_length,
            UART_TX_TIMEOUT_MS) != HAL_OK) {
        ++stats.transmit_failures;
        return MESSAGE_PROTOCOL_IO_ERROR;
    }
    ++stats.transmitted;
    return MESSAGE_PROTOCOL_OK;
}

static bool stop_audio(void)
{
    audio_status = audio_service_stop();
    return audio_status == VS1003B_STATUS_OK;
}

static bool play_audio(message_type_t message)
{
    audio_status = audio_service_play(message);
    return audio_status == VS1003B_STATUS_OK;
}

static void start_fall_failsafe(void)
{
    fall_failsafe_active = true;
    alert_show(MSG_FALL_DETECTED);
    buzzer_play_pattern(BUZZER_PATTERN_EMERGENCY);
    display_service_set_fall(true);
    (void)stop_audio();
    (void)play_audio(MSG_FALL_DETECTED);
}

static uint32_t request_id(const nostos_message_t *message)
{
    if (message->type == NOSTOS_MESSAGE_PACE_REQUEST) {
        return message->payload.pace_request.request_id;
    }
    if (message->type == NOSTOS_MESSAGE_STOP_REQUEST) {
        return message->payload.stop_request.request_id;
    }
    return 0U;
}

static bool request_seen(const nostos_message_t *message)
{
    uint32_t id = request_id(message);
    if (id == 0U) return false;

    for (size_t index = 0U; index < RECENT_REQUEST_CAPACITY; ++index) {
        if (recent_requests[index].request_id == id &&
            recent_requests[index].source_node_id == message->source_node_id &&
            recent_requests[index].type == message->type) {
            return true;
        }
    }
    return false;
}

static void remember_request(const nostos_message_t *message)
{
    recent_requests[recent_request_next] = (recent_request_t){
        .request_id = request_id(message),
        .source_node_id = message->source_node_id,
        .type = message->type,
    };
    recent_request_next =
        (recent_request_next + 1U) % RECENT_REQUEST_CAPACITY;
}

static bool execute_state_update(const nostos_message_t *message)
{
    const nostos_state_update_t *state = &message->payload.state_update;
    if (state->topic_id == NOSTOS_TOPIC_RIDE) {
        if (state->value.ride.trip_distance_m > UINT32_MAX / 1000U) {
            return false;
        }
        return sensor_view_service_apply_output_ride(
            message->source_node_id,
            state->sensor_valid,
            state->value.ride.speed_x10_kmh,
            state->value.ride.trip_distance_m * 1000U,
            HAL_GetTick());
    }
    if (state->topic_id == NOSTOS_TOPIC_ENVIRONMENT) {
        return sensor_view_service_apply_output_environment(
            message->source_node_id,
            state->sensor_valid,
            state->value.environment.temperature_x10_c,
            state->value.environment.humidity_x10_pct,
            HAL_GetTick());
    }
    return false;
}

static bool execute_pace_request(const nostos_message_t *message)
{
    message_type_t output = MSG_UNKNOWN;
    if (message->payload.pace_request.action == NOSTOS_PACE_ACCELERATE) {
        output = MSG_SPEED_UP_REQUEST;
    } else if (message->payload.pace_request.action == NOSTOS_PACE_DECELERATE) {
        output = MSG_SPEED_DOWN_REQUEST;
    } else {
        return false;
    }
    bool display_ok = display_service_show_button_message(
        message->source_node_id, (uint8_t)output);
    alert_show_local_button(output);
    bool audio_ok = stop_audio() && play_audio(output);
    return display_ok && audio_ok;
}

static bool execute_stop_request(const nostos_message_t *message)
{
    if (message->payload.stop_request.reason == NOSTOS_STOP_REASON_FALL) {
        start_fall_failsafe();
        return audio_status == VS1003B_STATUS_OK;
    }
    if (message->payload.stop_request.reason != NOSTOS_STOP_REASON_BUTTON) {
        return false;
    }
    bool display_ok = display_service_show_button_message(
        message->source_node_id, (uint8_t)MSG_STOP_REQUEST);
    alert_show_local_button(MSG_STOP_REQUEST);
    bool audio_ok = stop_audio() && play_audio(MSG_STOP_REQUEST);
    return display_ok && audio_ok;
}

static bool accept_stop_ack(const nostos_message_t *message)
{
    if (!pending_stop.active ||
        message->payload.stop_ack.request_id !=
            pending_stop.request.payload.stop_request.request_id) {
        return false;
    }
    clear_pending_stop();
    return true;
}

static message_protocol_result_t send_local_stop_ack(uint32_t request_id)
{
    nostos_message_t ack;
    if (nostos_message_make_stop_ack(&ack, NOSTOS_LOCAL_SOURCE_NODE_ID,
            request_id) != NOSTOS_OK) {
        return MESSAGE_PROTOCOL_BAD_VALUE;
    }
    return transmit_message(&ack);
}

static void handle_message(const nostos_message_t *message)
{
    ++stats.received;
    if (!nostos_node_id_valid(message->source_node_id)) {
        ++stats.rejected;
        return;
    }
    bool accepted = false;
    if (message->type == NOSTOS_MESSAGE_STATE_UPDATE) {
        accepted = execute_state_update(message);
        if (accepted) ++stats.state_updates;
    } else if (message->type == NOSTOS_MESSAGE_PACE_REQUEST) {
        if (request_seen(message)) {
            ++stats.duplicates;
            return;
        }
        remember_request(message);
        accepted = execute_pace_request(message);
        if (accepted) ++stats.pace_requests;
    } else if (message->type == NOSTOS_MESSAGE_STOP_REQUEST) {
        if (request_seen(message)) {
            ++stats.duplicates;
            (void)send_local_stop_ack(
                message->payload.stop_request.request_id);
            return;
        }
        /* A syntactically valid BUTTON/FALL request is accepted by the
         * application even when a display/audio output reports failure. */
        (void)execute_stop_request(message);
        remember_request(message);
        accepted = true;
        ++stats.stop_requests;
        (void)send_local_stop_ack(message->payload.stop_request.request_id);
    } else if (message->type == NOSTOS_MESSAGE_STOP_ACK) {
        accepted = true;
        ++stats.stop_acks;
        if (accept_stop_ack(message)) {
            ++stats.stop_ack_matches;
        } else {
            ++stats.stop_ack_ignored;
        }
    }
    if (!accepted) ++stats.rejected;
}

message_protocol_result_t message_protocol_service_boot(
    UART_HandleTypeDef *uart,
    vs1003b_status_t status)
{
    if (uart == NULL) return MESSAGE_PROTOCOL_BAD_ARGUMENT;

    data_uart = uart;
    audio_status = status;
    booted = true;
    fall_failsafe_active = false;
    clear_pending_stop();
    next_local_request_id = LOCAL_STOP_REQUEST_ID_INITIAL;
    stats = (message_protocol_stats_t){0};
    recent_request_next = 0U;
    for (size_t index = 0U; index < RECENT_REQUEST_CAPACITY; ++index) {
        recent_requests[index] = (recent_request_t){0};
    }
    reset_rx();
    alert_init();
    buzzer_init();
    sensor_view_service_clear_outputs();
    display_service_set_fall(false);
    display_service_set_link_ready(true);
    return MESSAGE_PROTOCOL_OK;
}

bool message_protocol_service_is_ready(void)
{
    return booted && data_uart != NULL;
}

const message_protocol_stats_t *message_protocol_service_stats(void)
{
    return &stats;
}

void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms)
{
    unsigned next = (rx_head + 1U) % RX_CAPACITY;
    if (next == rx_tail) {
        rx_overflow = true;
        return;
    }
    rx_bytes[rx_head] = byte;
    rx_times[rx_head] = received_ms;
    rx_head = next;
}

void message_protocol_service_rx_error_isr(void)
{
    rx_overflow = true;
}

void message_protocol_service_clear_pending(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_tail = rx_head;
    rx_overflow = false;
    __set_PRIMASK(mask);
    nostos_uart_reset(&parser);
    fall_failsafe_active = false;
    recent_request_next = 0U;
    for (size_t index = 0U; index < RECENT_REQUEST_CAPACITY; ++index) {
        recent_requests[index] = (recent_request_t){0};
    }
}

void message_protocol_service_process(void)
{
    for (unsigned budget = 0U; budget < RX_PROCESS_BUDGET; ++budget) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        if (rx_overflow) {
            rx_tail = rx_head;
            rx_overflow = false;
            __set_PRIMASK(mask);
            nostos_uart_reset(&parser);
            ++stats.overflows;
            break;
        }
        if (rx_tail == rx_head) {
            __set_PRIMASK(mask);
            break;
        }
        uint8_t byte = rx_bytes[rx_tail];
        uint32_t received_ms = rx_times[rx_tail];
        rx_tail = (rx_tail + 1U) % RX_CAPACITY;
        __set_PRIMASK(mask);

        nostos_message_t message;
        nostos_result_t result = nostos_uart_feed_message(
            &parser, byte, received_ms, &message);
        if (result == NOSTOS_OK) {
            stats.last_protocol_result = result;
            handle_message(&message);
        } else if (result != NOSTOS_EMPTY) {
            stats.last_protocol_result = result;
            ++stats.rejected;
        }
    }

    uint32_t now_ms = HAL_GetTick();
    if (pending_stop.active && due(now_ms, pending_stop.retry_at_ms)) {
        (void)transmit_message(&pending_stop.request);
        pending_stop.retry_at_ms = now_ms + STOP_RETRY_MS;
    }
    if (audio_status == VS1003B_STATUS_OK) {
        audio_status = audio_service_process();
    }
    alert_process();
    buzzer_process();
    if (fall_failsafe_active) display_service_set_fall(true);
}

message_protocol_result_t message_protocol_service_publish_event(
    uint8_t event_type)
{
    nostos_message_t message;
    nostos_result_t made;

    if (event_type == (uint8_t)MSG_SPEED_UP_REQUEST) {
        made = nostos_message_make_pace(&message, NOSTOS_LOCAL_SOURCE_NODE_ID,
            LOCAL_PACE_REQUEST_ID_PLACEHOLDER, NOSTOS_PACE_ACCELERATE);
    } else if (event_type == (uint8_t)MSG_SPEED_DOWN_REQUEST) {
        made = nostos_message_make_pace(&message, NOSTOS_LOCAL_SOURCE_NODE_ID,
            LOCAL_PACE_REQUEST_ID_PLACEHOLDER, NOSTOS_PACE_DECELERATE);
    } else {
        uint8_t reason = 0U;
        if (event_type == (uint8_t)MSG_STOP_REQUEST) {
            reason = NOSTOS_STOP_REASON_BUTTON;
        } else if (event_type == (uint8_t)MSG_FALL_DETECTED) {
            reason = NOSTOS_STOP_REASON_FALL;
            start_fall_failsafe();
        } else {
            return MESSAGE_PROTOCOL_BAD_VALUE;
        }

        bool replace = !pending_stop.active;
        if (pending_stop.active &&
            pending_stop.request.payload.stop_request.reason ==
                NOSTOS_STOP_REASON_BUTTON &&
            reason == NOSTOS_STOP_REASON_FALL) {
            replace = true;
        }

        if (replace) {
            made = nostos_message_make_stop(&message,
                NOSTOS_LOCAL_SOURCE_NODE_ID, take_local_request_id(), reason);
            stats.last_protocol_result = made;
            if (made != NOSTOS_OK) return MESSAGE_PROTOCOL_BAD_VALUE;
            pending_stop = (pending_stop_t){
                .active = true,
                .request = message,
                .retry_at_ms = HAL_GetTick() + STOP_RETRY_MS,
            };
        } else {
            message = pending_stop.request;
        }

        message_protocol_result_t result = transmit_message(&message);
        pending_stop.retry_at_ms = HAL_GetTick() + STOP_RETRY_MS;
        return result;
    }
    stats.last_protocol_result = made;
    if (made != NOSTOS_OK) return MESSAGE_PROTOCOL_BAD_VALUE;
    return transmit_message(&message);
}

#if defined(MESSAGE_PROTOCOL_TEST_PLATFORM_H)
void message_protocol_service_test_set_next_local_request_id(
    uint32_t request_id)
{
    next_local_request_id = request_id == 0U ?
        LOCAL_STOP_REQUEST_ID_INITIAL : request_id;
}
#endif

message_protocol_result_t message_protocol_service_publish_ride(
    bool sensor_valid,
    uint16_t speed_x10_kmh,
    uint32_t trip_distance_m)
{
    nostos_message_t message;
    nostos_result_t result = nostos_message_make_ride(
        &message, NOSTOS_LOCAL_SOURCE_NODE_ID, sensor_valid,
        speed_x10_kmh, trip_distance_m);
    stats.last_protocol_result = result;
    if (result != NOSTOS_OK) return MESSAGE_PROTOCOL_BAD_VALUE;
    return transmit_message(&message);
}

message_protocol_result_t message_protocol_service_publish_environment(
    bool sensor_valid,
    int16_t temperature_x10_c,
    uint16_t humidity_x10_pct)
{
    nostos_message_t message;
    nostos_result_t result = nostos_message_make_environment(
        &message, NOSTOS_LOCAL_SOURCE_NODE_ID, sensor_valid,
        temperature_x10_c, humidity_x10_pct);
    stats.last_protocol_result = result;
    if (result != NOSTOS_OK) return MESSAGE_PROTOCOL_BAD_VALUE;
    return transmit_message(&message);
}
