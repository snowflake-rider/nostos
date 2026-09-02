#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "display_service.h"
#include "message_protocol_service.h"
#include "nostos_uart.h"
#include "sensor_view_service.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

#define CAPTURE_CAPACITY 32U

typedef struct {
    uint8_t bytes[NOSTOS_UART_FRAME_MAX];
    size_t length;
} capture_t;

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static capture_t captures[CAPTURE_CAPACITY];
static capture_t failed_capture;
static size_t capture_count;
static unsigned transmit_failures;
static unsigned alert_fall_count;
static unsigned alert_button_count;
static message_type_t last_alert_button;
static unsigned buzzer_start_count;
static unsigned buzzer_stop_count;
static bool displayed_fall;
static bool displayed_link_ready;
static unsigned displayed_button_count;
static uint8_t displayed_sender;
static uint8_t displayed_type;
static bool display_button_result;
static unsigned audio_play_count;
static unsigned audio_stop_count;
static message_type_t last_audio_type;
static unsigned clear_outputs_count;
static unsigned ride_output_count;
static unsigned environment_output_count;
static uint8_t ride_source;
static bool ride_valid;
static uint16_t ride_speed;
static uint32_t ride_distance_mm;
static uint8_t environment_source;
static bool environment_valid;

static void reset_host_state(void)
{
    tick = 0U;
    capture_count = 0U;
    failed_capture = (capture_t){0};
    transmit_failures = 0U;
    alert_fall_count = 0U;
    alert_button_count = 0U;
    last_alert_button = MSG_UNKNOWN;
    buzzer_start_count = 0U;
    buzzer_stop_count = 0U;
    displayed_fall = false;
    displayed_link_ready = false;
    displayed_button_count = 0U;
    displayed_sender = 0U;
    displayed_type = 0U;
    display_button_result = true;
    audio_play_count = 0U;
    audio_stop_count = 0U;
    last_audio_type = MSG_UNKNOWN;
    clear_outputs_count = 0U;
    ride_output_count = 0U;
    environment_output_count = 0U;
    ride_source = 0U;
    ride_valid = false;
    ride_speed = 0U;
    ride_distance_mm = 0U;
    environment_source = 0U;
    environment_valid = false;
    for (size_t index = 0U; index < CAPTURE_CAPACITY; ++index) {
        captures[index] = (capture_t){0};
    }
}

uint32_t HAL_GetTick(void) { return tick; }

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef *uart,
    const uint8_t *bytes,
    uint16_t length,
    uint32_t timeout)
{
    CHECK(uart != NULL && uart->instance == 2U);
    CHECK(bytes != NULL && length <= NOSTOS_UART_FRAME_MAX);
    CHECK(timeout == 20U);
    if (transmit_failures != 0U) {
        --transmit_failures;
        failed_capture.length = length;
        for (size_t index = 0U; index < length; ++index) {
            failed_capture.bytes[index] = bytes[index];
        }
        return HAL_ERROR;
    }
    CHECK(capture_count < CAPTURE_CAPACITY);
    captures[capture_count].length = length;
    for (size_t index = 0U; index < length; ++index) {
        captures[capture_count].bytes[index] = bytes[index];
    }
    ++capture_count;
    return HAL_OK;
}

void alert_init(void) {}
void alert_reset(void) {}
void alert_show(message_type_t message)
{
    CHECK(message == MSG_FALL_DETECTED);
    ++alert_fall_count;
}
void alert_show_local_button(message_type_t message)
{
    ++alert_button_count;
    last_alert_button = message;
}
void alert_process(void) {}
alert_state_t alert_get_state(void) { return ALERT_STATE_OFF; }
bool alert_is_led_on(void) { return false; }

void buzzer_init(void) {}
void buzzer_play_pattern(buzzer_pattern_t pattern)
{
    CHECK(pattern == BUZZER_PATTERN_EMERGENCY);
    ++buzzer_start_count;
}
void buzzer_stop(void) { ++buzzer_stop_count; }
void buzzer_process(void) {}
bool buzzer_is_active(void) { return false; }
buzzer_pattern_t buzzer_get_pattern(void) { return BUZZER_PATTERN_NONE; }

bool audio_service_is_playing(void) { return false; }
uint32_t audio_service_position(void) { return 0U; }
vs1003b_status_t audio_service_play(message_type_t message)
{
    ++audio_play_count;
    last_audio_type = message;
    return VS1003B_STATUS_OK;
}
vs1003b_status_t audio_service_process(void) { return VS1003B_STATUS_OK; }
vs1003b_status_t audio_service_stop(void)
{
    ++audio_stop_count;
    return VS1003B_STATUS_OK;
}

void display_service_set_link_ready(bool ready) { displayed_link_ready = ready; }
void display_service_set_fall(bool active) { displayed_fall = active; }
bool display_service_show_button_message(uint8_t sender_id, uint8_t type)
{
    if (sender_id < 1U || sender_id > 10U) return false;
    ++displayed_button_count;
    displayed_sender = sender_id;
    displayed_type = type;
    return display_button_result;
}

void sensor_view_service_clear_outputs(void) { ++clear_outputs_count; }
bool sensor_view_service_apply_output_ride(
    uint8_t source_id,
    bool sensor_valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms)
{
    CHECK(now_ms == tick);
    ++ride_output_count;
    ride_source = source_id;
    ride_valid = sensor_valid;
    ride_speed = kmh_x10;
    ride_distance_mm = distance_mm;
    return true;
}
bool sensor_view_service_apply_output_environment(
    uint8_t source_id,
    bool sensor_valid,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint32_t now_ms)
{
    CHECK(now_ms == tick);
    CHECK(temperature_c_x10 == 253);
    CHECK(humidity_pct_x10 == 610U);
    ++environment_output_count;
    environment_source = source_id;
    environment_valid = sensor_valid;
    return true;
}

static bool decode_local_capture(size_t index, nostos_message_t *message)
{
    CHECK(index < capture_count && message != NULL);
    nostos_uart_parser_t capture_parser = {0};
    nostos_result_t result = NOSTOS_EMPTY;
    for (size_t byte = 0U; byte < captures[index].length; ++byte) {
        result = nostos_uart_feed_local_message(&capture_parser,
            captures[index].bytes[byte], (uint32_t)byte, message);
    }
    return result == NOSTOS_OK;
}

static void receive_frame(const uint8_t *frame, size_t length, uint32_t start_ms)
{
    for (size_t index = 0U; index < length; ++index) {
        message_protocol_service_rx_isr(frame[index],
            start_ms + (uint32_t)index);
    }
    tick = start_ms + (uint32_t)length;
    message_protocol_service_process();
}

static void encode_remote(
    const nostos_message_t *message,
    uint8_t frame[NOSTOS_UART_FRAME_MAX],
    size_t *length)
{
    CHECK(nostos_uart_encode_message(
        message, frame, NOSTOS_UART_FRAME_MAX, length) == NOSTOS_OK);
}

static nostos_message_t captured_message(size_t index)
{
    nostos_message_t message;
    CHECK(decode_local_capture(index, &message));
    return message;
}

static void receive_stop_ack(uint8_t source_node_id, uint32_t request_id)
{
    nostos_message_t message;
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t length = 0U;
    CHECK(nostos_message_make_stop_ack(
        &message, source_node_id, request_id) == NOSTOS_OK);
    encode_remote(&message, frame, &length);
    receive_frame(frame, length, tick + 1U);
}

static void boot_is_stateless(UART_HandleTypeDef *uart)
{
    tick = 100U;
    CHECK(message_protocol_service_boot(uart, VS1003B_STATUS_OK) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(message_protocol_service_is_ready());
    CHECK(displayed_link_ready);
    CHECK(clear_outputs_count == 1U);
    CHECK(capture_count == 0U); /* No HELLO/READY or session handshake. */
}

static void local_producers_use_source_zero(void)
{
    nostos_message_t first;
    CHECK(message_protocol_service_publish_event(MSG_SPEED_UP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(decode_local_capture(0U, &first));
    CHECK(first.type == NOSTOS_MESSAGE_PACE_REQUEST);
    CHECK(first.source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID);
    CHECK(first.payload.pace_request.request_id != 0U);
    CHECK(first.payload.pace_request.action == NOSTOS_PACE_ACCELERATE);

    nostos_message_t second;
    CHECK(message_protocol_service_publish_event(MSG_SPEED_DOWN_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(decode_local_capture(1U, &second));
    CHECK(second.payload.pace_request.request_id != 0U);
    CHECK(second.payload.pace_request.action == NOSTOS_PACE_DECELERATE);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(decode_local_capture(2U, &second));
    CHECK(second.type == NOSTOS_MESSAGE_STOP_REQUEST);
    CHECK(second.payload.stop_request.reason == NOSTOS_STOP_REASON_BUTTON);

    CHECK(message_protocol_service_publish_ride(true, 205U, 1234U) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(decode_local_capture(3U, &second));
    CHECK(second.type == NOSTOS_MESSAGE_STATE_UPDATE);
    CHECK(second.payload.state_update.topic_id == NOSTOS_TOPIC_RIDE);
    CHECK(second.payload.state_update.value.ride.trip_distance_m == 1234U);

    CHECK(message_protocol_service_publish_environment(true, 253, 610U) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(decode_local_capture(4U, &second));
    CHECK(second.payload.state_update.topic_id == NOSTOS_TOPIC_ENVIRONMENT);
    CHECK(second.payload.state_update.sensor_valid);

    CHECK(message_protocol_service_publish_event(0x99U) ==
        MESSAGE_PROTOCOL_BAD_VALUE);
}

static void stop_retries_until_matching_paired_ack(UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t first = captured_message(0U);
    uint32_t request_id = first.payload.stop_request.request_id;
    CHECK(first.type == NOSTOS_MESSAGE_STOP_REQUEST);
    CHECK(first.payload.stop_request.reason == NOSTOS_STOP_REASON_BUTTON);

    tick += 199U;
    message_protocol_service_process();
    CHECK(capture_count == 1U);
    tick += 1U;
    message_protocol_service_process();
    CHECK(capture_count == 2U);
    nostos_message_t retry = captured_message(1U);
    CHECK(retry.payload.stop_request.request_id == request_id);

    receive_stop_ack(2U, request_id + 1U);
    CHECK(message_protocol_service_stats()->stop_ack_ignored == 1U);
    tick += 200U;
    message_protocol_service_process();
    CHECK(capture_count == 3U);
    retry = captured_message(2U);
    CHECK(retry.payload.stop_request.request_id == request_id);

    receive_stop_ack(2U, request_id);
    CHECK(message_protocol_service_stats()->stop_ack_matches == 1U);
    size_t before = capture_count;
    tick += 1000U;
    message_protocol_service_process();
    CHECK(capture_count == before);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t second = captured_message(before);
    CHECK(second.payload.stop_request.request_id != request_id);
    receive_stop_ack(3U, second.payload.stop_request.request_id);
    CHECK(message_protocol_service_stats()->stop_ack_matches == 2U);
    tick += 200U;
    message_protocol_service_process();
    CHECK(capture_count == before + 1U);
}

static void stop_merge_prefers_fall_and_rejects_old_ack(
    UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t button = captured_message(0U);
    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t button_again = captured_message(1U);
    CHECK(button_again.payload.stop_request.request_id ==
        button.payload.stop_request.request_id);

    CHECK(message_protocol_service_publish_event(MSG_FALL_DETECTED) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t fall = captured_message(2U);
    CHECK(fall.payload.stop_request.reason == NOSTOS_STOP_REASON_FALL);
    CHECK(fall.payload.stop_request.request_id !=
        button.payload.stop_request.request_id);

    receive_stop_ack(2U, button.payload.stop_request.request_id);
    CHECK(message_protocol_service_stats()->stop_ack_ignored == 1U);
    tick += 200U;
    message_protocol_service_process();
    nostos_message_t fall_retry = captured_message(3U);
    CHECK(fall_retry.payload.stop_request.request_id ==
        fall.payload.stop_request.request_id);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t fall_kept = captured_message(4U);
    CHECK(fall_kept.payload.stop_request.request_id ==
        fall.payload.stop_request.request_id);
    CHECK(fall_kept.payload.stop_request.reason == NOSTOS_STOP_REASON_FALL);

    CHECK(message_protocol_service_publish_event(MSG_FALL_DETECTED) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t fall_again = captured_message(5U);
    CHECK(fall_again.payload.stop_request.request_id ==
        fall.payload.stop_request.request_id);
    receive_stop_ack(2U, fall.payload.stop_request.request_id);
    size_t before = capture_count;
    tick += 200U;
    message_protocol_service_process();
    CHECK(capture_count == before);
}

static void stop_requests_retry_uart_failure(UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);
    transmit_failures = 1U;
    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_IO_ERROR);
    size_t before = capture_count;
    tick += 199U;
    message_protocol_service_process();
    CHECK(capture_count == before);
    tick += 1U;
    message_protocol_service_process();
    CHECK(capture_count == before + 1U);
    nostos_message_t message;
    CHECK(decode_local_capture(before, &message));
    CHECK(message.type == NOSTOS_MESSAGE_STOP_REQUEST);
    CHECK(message.payload.stop_request.reason == NOSTOS_STOP_REASON_BUTTON);
    CHECK(message.payload.stop_request.request_id != 0U);
    CHECK(failed_capture.length == captures[before].length);
    for (size_t index = 0U; index < failed_capture.length; ++index) {
        CHECK(failed_capture.bytes[index] == captures[before].bytes[index]);
    }
    receive_stop_ack(2U, message.payload.stop_request.request_id);
}

static void stop_id_wrap_and_boot_reset(UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);
    message_protocol_service_test_set_next_local_request_id(UINT32_MAX);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t last = captured_message(0U);
    CHECK(last.payload.stop_request.request_id == UINT32_MAX);
    receive_stop_ack(2U, UINT32_MAX);

    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t wrapped = captured_message(1U);
    CHECK(wrapped.payload.stop_request.request_id == 1U);

    reset_host_state();
    boot_is_stateless(uart);
    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t reset = captured_message(0U);
    CHECK(reset.payload.stop_request.request_id == 1U);
}

static void output_clear_preserves_pending_stop(UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);
    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t first = captured_message(0U);

    message_protocol_service_clear_pending();
    tick += 200U;
    message_protocol_service_process();
    CHECK(capture_count == 2U);
    nostos_message_t retry = captured_message(1U);
    CHECK(retry.payload.stop_request.request_id ==
        first.payload.stop_request.request_id);

    receive_stop_ack(3U, first.payload.stop_request.request_id);
    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t next = captured_message(2U);
    CHECK(next.payload.stop_request.request_id ==
        first.payload.stop_request.request_id + 1U);
}

static void stop_retry_deadline_wraps_tick(UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);
    tick = UINT32_MAX - 100U;
    CHECK(message_protocol_service_publish_event(MSG_STOP_REQUEST) ==
        MESSAGE_PROTOCOL_OK);
    nostos_message_t first = captured_message(0U);

    tick = 98U;
    message_protocol_service_process();
    CHECK(capture_count == 1U);
    tick = 99U;
    message_protocol_service_process();
    CHECK(capture_count == 2U);
    nostos_message_t retry = captured_message(1U);
    CHECK(retry.payload.stop_request.request_id ==
        first.payload.stop_request.request_id);
}

static void remote_stop_acceptance_ack_is_replayable(UART_HandleTypeDef *uart)
{
    reset_host_state();
    boot_is_stateless(uart);
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t length = 0U;
    nostos_message_t request;

    CHECK(nostos_message_make_stop(&request, 4U, 500U,
        NOSTOS_STOP_REASON_BUTTON) == NOSTOS_OK);
    encode_remote(&request, frame, &length);
    display_button_result = false;
    receive_frame(frame, length, 2000U);
    CHECK(displayed_button_count == 1U);
    CHECK(audio_play_count == 1U);
    CHECK(message_protocol_service_stats()->stop_requests == 1U);
    CHECK(capture_count == 1U);
    nostos_message_t ack = captured_message(0U);
    CHECK(ack.type == NOSTOS_MESSAGE_STOP_ACK);
    CHECK(ack.source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID);
    CHECK(ack.payload.stop_ack.request_id == 500U);

    unsigned displays = displayed_button_count;
    unsigned plays = audio_play_count;
    receive_frame(frame, length, 2100U);
    CHECK(displayed_button_count == displays);
    CHECK(audio_play_count == plays);
    CHECK(capture_count == 2U);
    ack = captured_message(1U);
    CHECK(ack.payload.stop_ack.request_id == 500U);

    CHECK(nostos_message_make_stop(&request, 5U, 501U,
        NOSTOS_STOP_REASON_FALL) == NOSTOS_OK);
    encode_remote(&request, frame, &length);
    transmit_failures = 1U;
    receive_frame(frame, length, 2200U);
    CHECK(alert_fall_count == 1U);
    CHECK(capture_count == 2U);
    uint32_t accepted = message_protocol_service_stats()->stop_requests;
    receive_frame(frame, length, 2300U);
    CHECK(alert_fall_count == 1U);
    CHECK(message_protocol_service_stats()->stop_requests == accepted);
    CHECK(capture_count == 3U);
    ack = captured_message(2U);
    CHECK(ack.source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID);
    CHECK(ack.payload.stop_ack.request_id == 501U);
}

static void remote_messages_drive_outputs_and_dedupe(void)
{
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t length = 0U;
    nostos_message_t message;

    CHECK(nostos_message_make_ride(&message, 3U, true, 310U, 41U) ==
        NOSTOS_OK);
    encode_remote(&message, frame, &length);
    receive_frame(frame, length, 1000U);
    CHECK(ride_output_count == 1U && ride_source == 3U && ride_valid);
    CHECK(ride_speed == 310U && ride_distance_mm == 41000U);

    CHECK(nostos_message_make_environment(&message, 10U, true, 253, 610U) ==
        NOSTOS_OK);
    encode_remote(&message, frame, &length);
    receive_frame(frame, length, 1100U);
    CHECK(environment_output_count == 1U && environment_source == 10U);
    CHECK(environment_valid);

    CHECK(nostos_message_make_pace(&message, 4U, 77U,
        NOSTOS_PACE_DECELERATE) == NOSTOS_OK);
    encode_remote(&message, frame, &length);
    receive_frame(frame, length, 1200U);
    CHECK(displayed_sender == 4U && displayed_type == MSG_SPEED_DOWN_REQUEST);
    CHECK(last_alert_button == MSG_SPEED_DOWN_REQUEST);
    CHECK(last_audio_type == MSG_SPEED_DOWN_REQUEST);
    unsigned displays = displayed_button_count;
    unsigned plays = audio_play_count;
    receive_frame(frame, length, 1300U);
    CHECK(displayed_button_count == displays);
    CHECK(audio_play_count == plays);
    CHECK(message_protocol_service_stats()->duplicates == 1U);

    CHECK(nostos_message_make_stop(&message, 10U, 88U,
        NOSTOS_STOP_REASON_BUTTON) == NOSTOS_OK);
    encode_remote(&message, frame, &length);
    receive_frame(frame, length, 1400U);
    CHECK(displayed_sender == 10U && displayed_type == MSG_STOP_REQUEST);

    unsigned fall_count = alert_fall_count;
    CHECK(nostos_message_make_stop(&message, 2U, 99U,
        NOSTOS_STOP_REASON_FALL) == NOSTOS_OK);
    encode_remote(&message, frame, &length);
    receive_frame(frame, length, 1500U);
    CHECK(alert_fall_count == fall_count + 1U);
    CHECK(last_audio_type == MSG_FALL_DETECTED);
}

static void malformed_and_local_source_frames_are_rejected(void)
{
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t length = 0U;
    nostos_message_t message;
    CHECK(nostos_message_make_pace(&message, NOSTOS_LOCAL_SOURCE_NODE_ID,
        123U, NOSTOS_PACE_ACCELERATE) == NOSTOS_OK);
    CHECK(nostos_uart_encode_local_message(&message, frame, sizeof(frame),
        &length) == NOSTOS_OK);
    uint32_t rejected = message_protocol_service_stats()->rejected;
    receive_frame(frame, length, 1600U);
    CHECK(message_protocol_service_stats()->rejected > rejected);

    CHECK(nostos_message_make_pace(&message, 1U, 124U,
        NOSTOS_PACE_ACCELERATE) == NOSTOS_OK);
    encode_remote(&message, frame, &length);
    frame[length - 1U] ^= 0x01U;
    rejected = message_protocol_service_stats()->rejected;
    receive_frame(frame, length, 1700U);
    CHECK(message_protocol_service_stats()->rejected == rejected + 1U);
}

int main(void)
{
    UART_HandleTypeDef uart = {2U};
    reset_host_state();
    boot_is_stateless(&uart);
    local_producers_use_source_zero();
    stop_retries_until_matching_paired_ack(&uart);
    stop_merge_prefers_fall_and_rejects_old_ack(&uart);
    stop_requests_retry_uart_failure(&uart);
    stop_id_wrap_and_boot_reset(&uart);
    output_clear_preserves_pending_stop(&uart);
    stop_retry_deadline_wraps_tick(&uart);
    remote_stop_acceptance_ack_is_replayable(&uart);
    reset_host_state();
    boot_is_stateless(&uart);
    remote_messages_drive_outputs_and_dedupe();
    malformed_and_local_source_frames_are_rejected();
    CHECK(message_protocol_service_stats()->pace_requests == 1U);
    CHECK(message_protocol_service_stats()->stop_requests == 2U);
    CHECK(message_protocol_service_stats()->state_updates == 2U);
    CHECK(audio_stop_count > 0U && buzzer_stop_count == 0U);
    puts("PASS stateless A5 5A UART integration, local source stamping boundary");
    puts("PASS STOP retry-until-ACK, FALL priority merge, wrap/reset identity");
    puts("PASS state cache updates, pace/stop outputs, RAM dedupe");
    puts("HARDWARE_UART_AUDIO_DISPLAY_MESH=NOT_TESTED");
    return 0;
}
