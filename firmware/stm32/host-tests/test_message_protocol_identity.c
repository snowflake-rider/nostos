#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "display_service.h"
#include "message_protocol_service.h"
#include "sensor_view_service.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

#define CAPTURE_CAPACITY 96U

typedef struct {
    uint8_t bytes[SENSOR_LINK_FRAME_SIZE];
    size_t length;
} capture_t;

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static capture_t captures[CAPTURE_CAPACITY];
static size_t capture_count;
static unsigned transmit_failures;

static unsigned alert_reset_count;
static unsigned alert_show_count;
static unsigned alert_button_count;
static message_type_t last_alert_button;
static unsigned buzzer_start_count;
static unsigned buzzer_stop_count;
static bool displayed_fall;
static bool displayed_link_ready;
static unsigned displayed_button_count;
static uint8_t displayed_sender;
static uint8_t displayed_type;
static unsigned audio_play_count;
static unsigned audio_stop_count;
static message_type_t last_audio_type;
static vs1003b_status_t audio_play_result = VS1003B_STATUS_OK;
static vs1003b_status_t audio_stop_result = VS1003B_STATUS_OK;
static unsigned ride_output_count;
static uint8_t ride_output_source;
static bool ride_output_valid;
static uint16_t ride_output_speed;
static uint32_t ride_output_distance;
static unsigned environment_output_count;
static uint8_t environment_output_source;

uint32_t HAL_GetTick(void)
{
    return tick;
}

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef *uart,
    const uint8_t *bytes,
    uint16_t length,
    uint32_t timeout)
{
    CHECK(uart != NULL && uart->instance == 2U);
    CHECK(bytes != NULL && length <= SENSOR_LINK_FRAME_SIZE);
    CHECK(timeout == 20U);
    if (transmit_failures != 0U) {
        --transmit_failures;
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
void alert_reset(void) { ++alert_reset_count; }
void alert_show(message_type_t message)
{
    CHECK(message == MSG_FALL_DETECTED);
    ++alert_show_count;
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
    return audio_play_result;
}
vs1003b_status_t audio_service_process(void) { return VS1003B_STATUS_OK; }
vs1003b_status_t audio_service_stop(void)
{
    ++audio_stop_count;
    return audio_stop_result;
}

void display_service_set_link_ready(bool ready)
{
    displayed_link_ready = ready;
}
void display_service_set_fall(bool active) { displayed_fall = active; }
bool display_service_show_button_message(uint8_t sender_id, uint8_t type)
{
    ++displayed_button_count;
    displayed_sender = sender_id;
    displayed_type = type;
    return sender_id >= SENSOR_LINK_SOURCE_ID_MIN &&
        sender_id <= SENSOR_LINK_SOURCE_ID_MAX;
}

bool sensor_view_service_apply_output_ride(
    uint8_t source_id,
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms)
{
    CHECK(now_ms == tick);
    ++ride_output_count;
    ride_output_source = source_id;
    ride_output_valid = valid;
    ride_output_speed = kmh_x10;
    ride_output_distance = distance_mm;
    return true;
}

bool sensor_view_service_apply_output_environment(
    uint8_t source_id,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint32_t now_ms)
{
    CHECK(temperature_c_x10 == 253);
    CHECK(humidity_pct_x10 == 610U);
    CHECK(temperature_quality == SENSOR_LINK_QUALITY_VALID);
    CHECK(humidity_quality == SENSOR_LINK_QUALITY_VALID);
    CHECK(now_ms == tick);
    ++environment_output_count;
    environment_output_source = source_id;
    return true;
}

void sensor_view_service_clear_outputs(void) {}

static bool decode_capture(size_t index, sensor_link_message_t *message)
{
    if (index >= capture_count || message == NULL) return false;
    sensor_link_parser_t capture_parser = {0};
    sensor_link_result_t result = SENSOR_LINK_EMPTY;
    for (size_t byte = 0U; byte < captures[index].length; ++byte) {
        result = sensor_link_feed(&capture_parser,
            captures[index].bytes[byte], (uint32_t)byte, message);
    }
    return result == SENSOR_LINK_OK;
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

static uint8_t captured_result_status(size_t index, uint32_t command_id)
{
    sensor_link_message_t decoded;
    CHECK(decode_capture(index, &decoded));
    CHECK(decoded.type == SENSOR_LINK_OUTPUT_RESULT);
    CHECK(decoded.output_result.command_id == command_id);
    return decoded.output_result.status;
}

static void boot_hello_ready_and_no_official_parser(UART_HandleTypeDef *uart)
{
    tick = 100U;
    CHECK(message_protocol_service_boot(uart, VS1003B_STATUS_OK) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(!message_protocol_service_is_ready());
    CHECK(!displayed_link_ready);
    CHECK(capture_count == 1U);
    sensor_link_message_t decoded;
    CHECK(decode_capture(0U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_HELLO);

    CHECK(message_protocol_service_publish_event(
        SENSOR_LINK_EVENT_SPEED_UP) == MESSAGE_PROTOCOL_NOT_READY);
    CHECK(capture_count == 1U);
    CHECK(alert_button_count == 0U && audio_play_count == 0U);

    unsigned fall_alerts = alert_show_count;
    unsigned fall_buzzers = buzzer_start_count;
    CHECK(message_protocol_service_publish_event(SENSOR_LINK_EVENT_FALL) ==
        MESSAGE_PROTOCOL_NOT_READY);
    CHECK(capture_count == 1U);
    CHECK(alert_show_count == fall_alerts + 1U);
    CHECK(buzzer_start_count == fall_buzzers + 1U);
    CHECK(displayed_fall);

    tick = 1099U;
    message_protocol_service_process();
    CHECK(capture_count == 1U);
    tick = 1100U;
    message_protocol_service_process();
    CHECK(capture_count == 2U);
    CHECK(decode_capture(1U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_HELLO);

    const uint8_t official_packet[] = {0x7EU, 0x02U, 0x11U, 0x7EU};
    receive_frame(official_packet, sizeof(official_packet), 1120U);
    CHECK(capture_count == 2U);
    CHECK(message_protocol_service_stats()->received == 0U);

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    CHECK(sensor_link_encode_ready(77U, frame, &length) == SENSOR_LINK_OK);
    receive_frame(frame, length, 1200U);
    CHECK(message_protocol_service_is_ready());
    CHECK(displayed_link_ready);
    CHECK(message_protocol_service_stats()->ready_received == 1U);
    CHECK(capture_count == 3U);
    CHECK(decode_capture(2U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_EVENT);
    CHECK(decoded.event.type == SENSOR_LINK_EVENT_FALL);

    tick = 2300U;
    message_protocol_service_process();
    CHECK(capture_count == 3U);
}

static void producer_frames_and_fall_failsafe(void)
{
    size_t capture = capture_count;
    CHECK(message_protocol_service_publish_event(
        SENSOR_LINK_EVENT_SPEED_UP) == MESSAGE_PROTOCOL_OK);
    sensor_link_message_t decoded;
    CHECK(decode_capture(capture, &decoded));
    CHECK(decoded.type == SENSOR_LINK_EVENT);
    CHECK(decoded.event.type == SENSOR_LINK_EVENT_SPEED_UP);
    CHECK(alert_button_count == 0U && audio_play_count == 0U);

    capture = capture_count;
    CHECK(message_protocol_service_publish_ride(true, 300U, 1234567U) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(decode_capture(capture, &decoded));
    CHECK(decoded.type == SENSOR_LINK_RIDE);
    CHECK(decoded.ride.valid && decoded.ride.kmh_x10 == 300U);
    CHECK(decoded.ride.distance_mm == 1234567U);

    capture = capture_count;
    CHECK(message_protocol_service_publish_environment(
        253, 610U, SENSOR_LINK_QUALITY_VALID) == MESSAGE_PROTOCOL_OK);
    CHECK(decode_capture(capture, &decoded));
    CHECK(decoded.type == SENSOR_LINK_ENVIRONMENT);
    CHECK(decoded.environment.temperature_c_x10 == 253);
    CHECK(decoded.environment.humidity_pct_x10 == 610U);

    CHECK(message_protocol_service_publish_event(0x99U) ==
        MESSAGE_PROTOCOL_BAD_VALUE);
    CHECK(message_protocol_service_publish_ride(false, 1U, 0U) ==
        MESSAGE_PROTOCOL_BAD_VALUE);

    /* FALL is the sole local output exception and activates immediately. */
    unsigned fall_alerts = alert_show_count;
    unsigned fall_buzzers = buzzer_start_count;
    CHECK(message_protocol_service_publish_event(SENSOR_LINK_EVENT_FALL) ==
        MESSAGE_PROTOCOL_OK);
    CHECK(alert_show_count == fall_alerts + 1U);
    CHECK(buzzer_start_count == fall_buzzers + 1U);
    CHECK(displayed_fall);
}

static void output_commands_are_idempotent_and_exact(void)
{
    const uint8_t events[] = {
        SENSOR_LINK_EVENT_SPEED_UP,
        SENSOR_LINK_EVENT_SPEED_DOWN,
        SENSOR_LINK_EVENT_STOP,
    };
    const message_type_t messages[] = {
        MSG_SPEED_UP_REQUEST,
        MSG_SPEED_DOWN_REQUEST,
        MSG_STOP_REQUEST,
    };
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;

    for (size_t index = 0U; index < 3U; ++index) {
        uint32_t command_id = (uint32_t)index + 1U;
        CHECK(sensor_link_encode_output_event(command_id, 1U, events[index],
            frame, &length) == SENSOR_LINK_OK);
        size_t result_capture = capture_count;
        unsigned plays = audio_play_count;
        receive_frame(frame, length, 3000U + (uint32_t)index * 100U);
        CHECK(captured_result_status(result_capture, command_id) ==
            SENSOR_LINK_OUTPUT_ACCEPTED);
        CHECK(audio_play_count == plays + 1U);
        CHECK(last_audio_type == messages[index]);
        CHECK(last_alert_button == messages[index]);
        CHECK(displayed_sender == 1U && displayed_type == events[index]);
        CHECK(!displayed_fall);
    }

    unsigned plays = audio_play_count;
    unsigned displays = displayed_button_count;
    CHECK(sensor_link_encode_output_event(3U, 1U, SENSOR_LINK_EVENT_STOP,
        frame, &length) == SENSOR_LINK_OK);
    size_t duplicate_capture = capture_count;
    receive_frame(frame, length, 3400U);
    CHECK(captured_result_status(duplicate_capture, 3U) ==
        SENSOR_LINK_OUTPUT_DUPLICATE);
    CHECK(audio_play_count == plays);
    CHECK(displayed_button_count == displays);

    /* Repeated READY in the same epoch must not reset the command floor. */
    CHECK(sensor_link_encode_ready(77U, frame, &length) == SENSOR_LINK_OK);
    receive_frame(frame, length, 3500U);
    duplicate_capture = capture_count;
    CHECK(sensor_link_encode_output_event(2U, 1U,
        SENSOR_LINK_EVENT_SPEED_DOWN, frame, &length) == SENSOR_LINK_OK);
    receive_frame(frame, length, 3600U);
    CHECK(captured_result_status(duplicate_capture, 2U) ==
        SENSOR_LINK_OUTPUT_DUPLICATE);
    CHECK(audio_play_count == plays);

    CHECK(sensor_link_encode_output_ride(4U, 3U, true, 228U, 654321U,
        frame, &length) == SENSOR_LINK_OK);
    size_t result_capture = capture_count;
    receive_frame(frame, length, 3700U);
    CHECK(captured_result_status(result_capture, 4U) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);
    CHECK(ride_output_count == 1U && ride_output_source == 3U);
    CHECK(ride_output_valid && ride_output_speed == 228U);
    CHECK(ride_output_distance == 654321U);

    CHECK(sensor_link_encode_output_environment(5U, 2U, 253, 610U,
        SENSOR_LINK_QUALITY_VALID, SENSOR_LINK_QUALITY_VALID,
        frame, &length) == SENSOR_LINK_OK);
    result_capture = capture_count;
    receive_frame(frame, length, 3800U);
    CHECK(captured_result_status(result_capture, 5U) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);
    CHECK(environment_output_count == 1U && environment_output_source == 2U);

    CHECK(sensor_link_encode_output_event(6U, 2U,
        SENSOR_LINK_EVENT_FALL, frame, &length) == SENSOR_LINK_OK);
    result_capture = capture_count;
    receive_frame(frame, length, 3850U);
    CHECK(captured_result_status(result_capture, 6U) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);
    CHECK(displayed_fall);
    CHECK(last_audio_type == MSG_FALL_DETECTED);

    CHECK(sensor_link_encode_output_event(7U, 2U,
        SENSOR_LINK_EVENT_FALL_CLEAR, frame, &length) == SENSOR_LINK_OK);
    result_capture = capture_count;
    receive_frame(frame, length, 3875U);
    CHECK(captured_result_status(result_capture, 7U) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);
    CHECK(!displayed_fall);

    /* A stale ID is duplicate even when the command type differs. */
    CHECK(sensor_link_encode_output_ride(4U, 1U, true, 999U, 999U,
        frame, &length) == SENSOR_LINK_OK);
    duplicate_capture = capture_count;
    receive_frame(frame, length, 3900U);
    CHECK(captured_result_status(duplicate_capture, 4U) ==
        SENSOR_LINK_OUTPUT_DUPLICATE);
    CHECK(ride_output_count == 1U);

    /* A new ESP command epoch explicitly permits command ID 1 again. */
    CHECK(sensor_link_encode_ready(88U, frame, &length) == SENSOR_LINK_OK);
    receive_frame(frame, length, 4000U);
    CHECK(message_protocol_service_stats()->ready_received == 2U);
    CHECK(sensor_link_encode_output_event(1U, 2U,
        SENSOR_LINK_EVENT_SPEED_UP, frame, &length) == SENSOR_LINK_OK);
    result_capture = capture_count;
    receive_frame(frame, length, 4100U);
    CHECK(captured_result_status(result_capture, 1U) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);

    audio_play_result = VS1003B_STATUS_SPI_ERROR;
    CHECK(sensor_link_encode_output_event(2U, 2U,
        SENSOR_LINK_EVENT_SPEED_DOWN, frame, &length) == SENSOR_LINK_OK);
    result_capture = capture_count;
    receive_frame(frame, length, 4200U);
    CHECK(captured_result_status(result_capture, 2U) ==
        SENSOR_LINK_OUTPUT_HARDWARE_ERROR);
    audio_play_result = VS1003B_STATUS_OK;

    unsigned output_calls = displayed_button_count + ride_output_count +
        environment_output_count;
    CHECK(sensor_link_encode_output_event(0U, 1U,
        SENSOR_LINK_EVENT_STOP, frame, &length) == SENSOR_LINK_BAD_VALUE);
    CHECK(displayed_button_count + ride_output_count +
        environment_output_count == output_calls);

    /* Producer/control frames arriving on RX are rejected without output. */
    CHECK(sensor_link_encode_event(SENSOR_LINK_EVENT_STOP,
        frame, &length) == SENSOR_LINK_OK);
    receive_frame(frame, length, 4300U);
    CHECK(displayed_button_count + ride_output_count +
        environment_output_count == output_calls);
    CHECK(message_protocol_service_stats()->output_rejected >= 1U);
}

static void bounded_ring_overflow_resets_parser(void)
{
    uint32_t before = message_protocol_service_stats()->overflows;
    for (size_t index = 0U; index < 512U; ++index) {
        message_protocol_service_rx_isr(0x55U, 5000U + (uint32_t)index);
    }
    tick = 5600U;
    message_protocol_service_process();
    CHECK(message_protocol_service_stats()->overflows == before + 1U);
}

int main(void)
{
    UART_HandleTypeDef uart = {2U};
    boot_hello_ready_and_no_official_parser(&uart);
    producer_frames_and_fall_failsafe();
    output_commands_are_idempotent_and_exact();
    bounded_ring_overflow_resets_parser();
    puts("PASS thin STM32 link: HELLO/READY epoch and no official 0x7E parser");
    puts("PASS producer frames, exact BTN1/2/3 output mapping, and FALL fail-safe");
    puts("PASS OUTPUT_RESULT, monotonic idempotency, invalid no-output, mirrors");
    return 0;
}
