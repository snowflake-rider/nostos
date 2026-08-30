#include "message_protocol_service.h"

#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "display_service.h"
#include "message_type.h"
#include "sensor_view_service.h"

#include <stddef.h>

#define RX_CAPACITY 512U
#define RX_PROCESS_BUDGET 64U
#define HELLO_PERIOD_MS 1000U
#define UART_TX_TIMEOUT_MS 20U

static UART_HandleTypeDef *data_uart;
static vs1003b_status_t audio_status;
static bool booted;
static volatile bool link_ready;
static bool local_failsafe_active;
static uint8_t pending_safety_event;
static sensor_link_parser_t parser;
static message_protocol_stats_t stats;
static uint32_t last_hello_ms;

static volatile uint8_t rx_bytes[RX_CAPACITY];
static volatile uint32_t rx_times[RX_CAPACITY];
static volatile unsigned rx_head;
static volatile unsigned rx_tail;
static volatile bool rx_overflow;

static uint32_t command_epoch;
static uint32_t last_command_id;

static void reset_rx(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow = false;
    __set_PRIMASK(mask);
    sensor_link_reset(&parser);
}

static bool transmit_frame(const uint8_t *frame, size_t length)
{
    if (data_uart == NULL || frame == NULL || length == 0U ||
        length > SENSOR_LINK_FRAME_SIZE) {
        return false;
    }

    /* At 115200/8N1 the largest 19-byte frame is under 2 ms. Main-loop only. */
    return HAL_UART_Transmit(data_uart, (uint8_t *)frame, (uint16_t)length,
        UART_TX_TIMEOUT_MS) == HAL_OK;
}

static message_protocol_result_t send_encoded(
    sensor_link_result_t encoded,
    const uint8_t *frame,
    size_t length)
{
    stats.last_link_result = encoded;
    if (encoded == SENSOR_LINK_BAD_ARGUMENT) {
        return MESSAGE_PROTOCOL_BAD_ARGUMENT;
    }
    if (encoded != SENSOR_LINK_OK) {
        return MESSAGE_PROTOCOL_BAD_VALUE;
    }
    return transmit_frame(frame, length) ?
        MESSAGE_PROTOCOL_OK : MESSAGE_PROTOCOL_IO_ERROR;
}

static message_protocol_result_t send_hello(uint32_t now_ms)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_hello(frame, &length);
    last_hello_ms = now_ms;
    message_protocol_result_t result = send_encoded(encoded, frame, length);
    if (result == MESSAGE_PROTOCOL_OK) {
        ++stats.hello_sent;
    } else {
        ++stats.hello_failures;
    }
    return result;
}

static bool send_output_result(uint32_t command_id, uint8_t status)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_output_result(
        command_id, status, frame, &length);
    if (encoded != SENSOR_LINK_OK || !transmit_frame(frame, length)) {
        ++stats.result_failures;
        stats.last_link_result = encoded;
        return false;
    }
    ++stats.result_sent;
    return true;
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

static void clear_emergency_output(void)
{
    alert_reset();
    buzzer_stop();
    display_service_set_fall(false);
}

static void start_fall_failsafe(void)
{
    local_failsafe_active = true;
    alert_show(MSG_FALL_DETECTED);
    buzzer_play_pattern(BUZZER_PATTERN_EMERGENCY);
    display_service_set_fall(true);
}

static uint8_t execute_output_event(const sensor_link_output_event_t *output)
{
    if (output == NULL || output->command_id == 0U ||
        output->source_id < SENSOR_LINK_SOURCE_ID_MIN ||
        output->source_id > SENSOR_LINK_SOURCE_ID_MAX) {
        return SENSOR_LINK_OUTPUT_REJECTED;
    }

    /* ESP32 commands are authoritative once they arrive. */
    local_failsafe_active = false;
    pending_safety_event = 0U;
    if (output->event_type == SENSOR_LINK_EVENT_FALL) {
        alert_show(MSG_FALL_DETECTED);
        buzzer_play_pattern(BUZZER_PATTERN_EMERGENCY);
        display_service_set_fall(true);
        return stop_audio() && play_audio(MSG_FALL_DETECTED) ?
            SENSOR_LINK_OUTPUT_ACCEPTED : SENSOR_LINK_OUTPUT_HARDWARE_ERROR;
    }

    clear_emergency_output();
    if (output->event_type == SENSOR_LINK_EVENT_FALL_CLEAR) {
        return stop_audio() ?
            SENSOR_LINK_OUTPUT_ACCEPTED : SENSOR_LINK_OUTPUT_HARDWARE_ERROR;
    }

    message_type_t message = MSG_UNKNOWN;
    if (output->event_type == SENSOR_LINK_EVENT_SPEED_UP) {
        message = MSG_SPEED_UP_REQUEST;
    } else if (output->event_type == SENSOR_LINK_EVENT_SPEED_DOWN) {
        message = MSG_SPEED_DOWN_REQUEST;
    } else if (output->event_type == SENSOR_LINK_EVENT_STOP) {
        message = MSG_STOP_REQUEST;
    } else {
        return SENSOR_LINK_OUTPUT_REJECTED;
    }

    bool display_ok = display_service_show_button_message(
        output->source_id, (uint8_t)message);
    alert_show_local_button(message);
    bool audio_ok = stop_audio() && play_audio(message);
    return display_ok && audio_ok ?
        SENSOR_LINK_OUTPUT_ACCEPTED : SENSOR_LINK_OUTPUT_HARDWARE_ERROR;
}

static uint8_t execute_output(const sensor_link_message_t *message)
{
    if (message == NULL) {
        return SENSOR_LINK_OUTPUT_REJECTED;
    }
    if (message->type == SENSOR_LINK_OUTPUT_EVENT) {
        return execute_output_event(&message->output_event);
    }
    if (message->type == SENSOR_LINK_OUTPUT_RIDE) {
        const sensor_link_output_ride_t *output = &message->output_ride;
        return sensor_view_service_apply_output_ride(
            output->source_id,
            output->valid,
            output->kmh_x10,
            output->distance_mm,
            HAL_GetTick()) ?
                SENSOR_LINK_OUTPUT_ACCEPTED : SENSOR_LINK_OUTPUT_REJECTED;
    }
    if (message->type == SENSOR_LINK_OUTPUT_ENVIRONMENT) {
        const sensor_link_output_environment_t *output =
            &message->output_environment;
        return sensor_view_service_apply_output_environment(
            output->source_id,
            output->temperature_c_x10,
            output->humidity_pct_x10,
            output->temperature_quality,
            output->humidity_quality,
            HAL_GetTick()) ?
                SENSOR_LINK_OUTPUT_ACCEPTED : SENSOR_LINK_OUTPUT_REJECTED;
    }
    return SENSOR_LINK_OUTPUT_REJECTED;
}

static uint32_t output_command_id(const sensor_link_message_t *message)
{
    if (message->type == SENSOR_LINK_OUTPUT_EVENT) {
        return message->output_event.command_id;
    }
    if (message->type == SENSOR_LINK_OUTPUT_RIDE) {
        return message->output_ride.command_id;
    }
    if (message->type == SENSOR_LINK_OUTPUT_ENVIRONMENT) {
        return message->output_environment.command_id;
    }
    return 0U;
}

static void handle_message(const sensor_link_message_t *message)
{
    ++stats.received;
    if (message->type == SENSOR_LINK_READY) {
        bool epoch_changed = message->ready.command_epoch != command_epoch;
        if (!link_ready || message->ready.command_epoch != command_epoch) {
            ++stats.ready_received;
        }
        if (epoch_changed) {
            command_epoch = message->ready.command_epoch;
            last_command_id = 0U;
            if (local_failsafe_active) {
                pending_safety_event = SENSOR_LINK_EVENT_FALL;
            }
        }
        link_ready = true;
        display_service_set_link_ready(true);
        return;
    }

    uint32_t command_id = output_command_id(message);
    if (command_id == 0U) {
        ++stats.rejected;
        ++stats.output_rejected;
        return;
    }
    if (!link_ready) {
        ++stats.output_rejected;
        (void)send_output_result(command_id, SENSOR_LINK_OUTPUT_REJECTED);
        return;
    }
    if (command_id <= last_command_id) {
        ++stats.output_duplicates;
        (void)send_output_result(command_id, SENSOR_LINK_OUTPUT_DUPLICATE);
        return;
    }

    uint8_t status = execute_output(message);
    last_command_id = command_id;
    if (status == SENSOR_LINK_OUTPUT_ACCEPTED) {
        ++stats.output_accepted;
    } else if (status == SENSOR_LINK_OUTPUT_HARDWARE_ERROR) {
        ++stats.output_hardware_errors;
    } else {
        ++stats.output_rejected;
    }
    (void)send_output_result(command_id, status);
}

message_protocol_result_t message_protocol_service_boot(
    UART_HandleTypeDef *uart,
    vs1003b_status_t status)
{
    if (uart == NULL) {
        return MESSAGE_PROTOCOL_BAD_ARGUMENT;
    }

    data_uart = uart;
    audio_status = status;
    booted = true;
    link_ready = false;
    local_failsafe_active = false;
    pending_safety_event = 0U;
    stats = (message_protocol_stats_t){0};
    command_epoch = 0U;
    last_command_id = 0U;
    reset_rx();
    alert_init();
    buzzer_init();
    sensor_view_service_clear_outputs();
    display_service_set_fall(false);
    display_service_set_link_ready(false);
    return send_hello(HAL_GetTick());
}

bool message_protocol_service_is_ready(void)
{
    return booted && link_ready;
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
        link_ready = false;
        return;
    }
    rx_bytes[rx_head] = byte;
    rx_times[rx_head] = received_ms;
    rx_head = next;
}

void message_protocol_service_rx_error_isr(void)
{
    rx_overflow = true;
    link_ready = false;
}

void message_protocol_service_clear_pending(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_tail = rx_head;
    rx_overflow = false;
    __set_PRIMASK(mask);
    sensor_link_reset(&parser);
    local_failsafe_active = false;
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
            sensor_link_reset(&parser);
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

        sensor_link_message_t message;
        sensor_link_result_t result = sensor_link_feed(
            &parser, byte, received_ms, &message);
        if (result == SENSOR_LINK_OK) {
            stats.last_link_result = result;
            handle_message(&message);
        } else if (result != SENSOR_LINK_EMPTY) {
            stats.last_link_result = result;
            ++stats.rejected;
        }
    }

    display_service_set_link_ready(link_ready);
    if (link_ready && pending_safety_event != 0U) {
        uint8_t frame[SENSOR_LINK_FRAME_SIZE];
        size_t length = 0U;
        sensor_link_result_t encoded = sensor_link_encode_event(
            pending_safety_event, frame, &length);
        if (send_encoded(encoded, frame, length) == MESSAGE_PROTOCOL_OK) {
            pending_safety_event = 0U;
        }
    }

    uint32_t now_ms = HAL_GetTick();
    if (booted && !link_ready &&
        (uint32_t)(now_ms - last_hello_ms) >= HELLO_PERIOD_MS) {
        display_service_set_link_ready(false);
        (void)send_hello(now_ms);
    }
    if (audio_status == VS1003B_STATUS_OK) {
        audio_status = audio_service_process();
    }
    alert_process();
    buzzer_process();
    if (local_failsafe_active) {
        display_service_set_fall(true);
    }
}

message_protocol_result_t message_protocol_service_publish_event(
    uint8_t event_type)
{
    if (event_type == SENSOR_LINK_EVENT_FALL) {
        start_fall_failsafe();
        pending_safety_event = event_type;
    } else if (event_type == SENSOR_LINK_EVENT_FALL_CLEAR) {
        local_failsafe_active = false;
        clear_emergency_output();
        (void)stop_audio();
        pending_safety_event = event_type;
    }

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_event(
        event_type, frame, &length);
    if (encoded != SENSOR_LINK_OK) {
        return send_encoded(encoded, frame, length);
    }
    if (!message_protocol_service_is_ready()) {
        return MESSAGE_PROTOCOL_NOT_READY;
    }
    message_protocol_result_t result = send_encoded(encoded, frame, length);
    if (result == MESSAGE_PROTOCOL_OK &&
        (event_type == SENSOR_LINK_EVENT_FALL ||
         event_type == SENSOR_LINK_EVENT_FALL_CLEAR)) {
        pending_safety_event = 0U;
    }
    return result;
}

message_protocol_result_t message_protocol_service_publish_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_ride(
        valid, kmh_x10, distance_mm, frame, &length);
    if (encoded != SENSOR_LINK_OK) {
        return send_encoded(encoded, frame, length);
    }
    if (!message_protocol_service_is_ready()) {
        return MESSAGE_PROTOCOL_NOT_READY;
    }
    return send_encoded(encoded, frame, length);
}

message_protocol_result_t message_protocol_service_publish_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t quality)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_environment(
        temperature_c_x10, humidity_pct_x10, quality, quality,
        frame, &length);
    if (encoded != SENSOR_LINK_OK) {
        return send_encoded(encoded, frame, length);
    }
    if (!message_protocol_service_is_ready()) {
        return MESSAGE_PROTOCOL_NOT_READY;
    }
    return send_encoded(encoded, frame, length);
}
