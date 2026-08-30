#include "message_protocol_service.h"
#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "sensor_link.h"
#include "sensor_store.h"
#include "sensor_view_service.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

#define TX_CAPTURE_COUNT 24U

typedef struct {
    uint8_t bytes[NOSTOS_UART_FRAME_MAX];
    size_t length;
} tx_capture_t;

static uint32_t tick;
static tx_capture_t transmissions[TX_CAPTURE_COUNT];
static size_t transmission_count;
static unsigned transmit_failures;
static uint32_t displayed_button_count;
static uint8_t displayed_sender;
static uint8_t displayed_type;
static bool displayed_fall;
static bool fake_audio_playing;
static uint32_t audio_play_count;
static uint32_t audio_stop_count;
static message_type_t last_audio_type;

uint32_t HAL_GetTick(void)
{
    return tick;
}

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef *uart,
    const uint8_t *data,
    uint16_t size,
    uint32_t timeout)
{
    (void)uart;
    (void)timeout;
    if (transmit_failures != 0U) {
        --transmit_failures;
        return HAL_ERROR;
    }
    if (transmission_count >= TX_CAPTURE_COUNT ||
        (size_t)size > sizeof(transmissions[0].bytes)) {
        return HAL_ERROR;
    }
    tx_capture_t *capture = &transmissions[transmission_count++];
    memcpy(capture->bytes, data, size);
    capture->length = size;
    return HAL_OK;
}

void alert_init(void) {}
void alert_reset(void) {}
void alert_show(message_type_t message) { (void)message; }
void alert_show_local_button(message_type_t message) { (void)message; }
void alert_process(void) {}
void buzzer_init(void) {}
void buzzer_play_pattern(buzzer_pattern_t pattern) { (void)pattern; }
void buzzer_stop(void) {}
void buzzer_process(void) {}
bool audio_service_is_playing(void) { return fake_audio_playing; }
vs1003b_status_t audio_service_play(message_type_t message)
{
    ++audio_play_count;
    last_audio_type = message;
    fake_audio_playing = true;
    return VS1003B_STATUS_OK;
}
vs1003b_status_t audio_service_process(void) { return VS1003B_STATUS_OK; }
vs1003b_status_t audio_service_stop(void)
{
    ++audio_stop_count;
    fake_audio_playing = false;
    return VS1003B_STATUS_OK;
}
void display_service_set_fall(bool active) { displayed_fall = active; }
bool display_service_show_button_message(uint8_t sender_id, uint8_t type)
{
    ++displayed_button_count;
    displayed_sender = sender_id;
    displayed_type = type;
    return true;
}

static bool decode_local_capture(size_t index, sensor_link_message_t *message)
{
    if (index >= transmission_count || message == NULL) {
        return false;
    }
    sensor_link_parser_t parser = {0};
    sensor_link_result_t result = SENSOR_LINK_EMPTY;
    for (size_t byte = 0U; byte < transmissions[index].length; ++byte) {
        result = sensor_link_feed(&parser, transmissions[index].bytes[byte],
            (uint32_t)byte, message);
    }
    return result == SENSOR_LINK_OK;
}

static bool decode_nostos_capture(size_t index, nostos_message_t *message)
{
    if (index >= transmission_count || message == NULL) {
        return false;
    }
    nostos_uart_parser_t parser = {0};
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t wire_length = 0U;
    nostos_result_t result = NOSTOS_EMPTY;
    for (size_t byte = 0U; byte < transmissions[index].length; ++byte) {
        result = nostos_uart_feed(&parser, transmissions[index].bytes[byte],
            (uint32_t)byte, wire, &wire_length);
    }
    return result == NOSTOS_OK &&
        nostos_message_decode(wire, wire_length, message) == NOSTOS_OK;
}

static void receive_local(const uint8_t *frame, size_t length, uint32_t start_ms)
{
    for (size_t index = 0U; index < length; ++index) {
        message_protocol_service_rx_isr(frame[index],
            start_ms + (uint32_t)index);
    }
}

static bool encode_nostos_frame(
    const nostos_message_t *message,
    uint8_t *frame,
    size_t *frame_length)
{
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t wire_length = 0U;
    return nostos_message_encode(message, wire, sizeof(wire), &wire_length) == NOSTOS_OK &&
        nostos_uart_encode(wire, wire_length, frame, NOSTOS_UART_FRAME_MAX,
            frame_length) == NOSTOS_OK;
}

static int boot_ride_and_identity(UART_HandleTypeDef *uart)
{
    sensor_store_init();
    sensor_view_service_init();
    tick = 100U;
    CHECK(message_protocol_service_boot(uart, VS1003B_STATUS_OK) == NOSTOS_OK);
    CHECK(message_protocol_service_endpoint() == NULL);
    CHECK(message_protocol_service_publish_ride(true, 100U, 1000U) ==
        NOSTOS_NOT_READY);
    CHECK(transmission_count == 1U);

    sensor_link_message_t decoded;
    CHECK(decode_local_capture(0U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_HELLO);

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    CHECK(sensor_link_encode_ride(true, 245U, 987654U, frame, &length) ==
        SENSOR_LINK_OK);
    receive_local(frame, length, 110U);
    tick = 130U;
    message_protocol_service_process();
    sensor_snapshot_t snapshot;
    CHECK(sensor_store_snapshot(tick, &snapshot));
    CHECK(snapshot.ride.quality == SENSOR_QUALITY_VALID);
    CHECK(snapshot.ride.kmh_x10 == 245U);
    CHECK(snapshot.ride.distance_mm == 987654U);
    CHECK(message_protocol_service_endpoint() == NULL);

    tick = 1099U;
    message_protocol_service_process();
    CHECK(transmission_count == 1U);
    tick = 1100U;
    message_protocol_service_process();
    CHECK(transmission_count == 2U);
    CHECK(decode_local_capture(1U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_HELLO);

    CHECK(sensor_link_encode_identity(2U, 500U, frame, &length) == SENSOR_LINK_OK);
    receive_local(frame, length, 1200U);
    tick = 1220U;
    message_protocol_service_process();
    nostos_endpoint_t *endpoint = message_protocol_service_endpoint();
    CHECK(endpoint != NULL);
    CHECK(endpoint->sender.source_id == 2U);
    CHECK(endpoint->sender.session_id == 500U);
    CHECK(transmission_count == 3U);
    CHECK(decode_local_capture(2U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_IDENTITY_ACK);
    CHECK(decoded.identity.source_id == 2U);
    CHECK(decoded.identity.session_id == 500U);

    CHECK(message_protocol_service_publish_ride(true, 300U, 1234567U) ==
        NOSTOS_OK);
    CHECK(transmission_count == 4U);
    nostos_message_t message;
    CHECK(decode_nostos_capture(3U, &message));
    CHECK(message.type == NOSTOS_RIDE);
    CHECK(message.source_id == 2U && message.session_id == 500U);
    CHECK(message.payload.ride.valid);
    CHECK(message.payload.ride.kmh_x10 == 300U);
    CHECK(message.payload.ride.distance_mm == 1234567U);
    CHECK(message_protocol_service_publish_ride(false, 1U, 0U) ==
        NOSTOS_BAD_VALUE);
    CHECK(message_protocol_service_publish_ride(false, 0U, 1U) ==
        NOSTOS_BAD_VALUE);
    CHECK(transmission_count == 4U);
    CHECK(message_protocol_service_publish_event(NOSTOS_SPEED_UP) == NOSTOS_OK);
    CHECK(transmission_count == 5U);
    CHECK(displayed_button_count == 1U);
    CHECK(displayed_sender == 2U);
    CHECK(displayed_type == NOSTOS_SPEED_UP);
    CHECK(audio_play_count == 1U);
    CHECK(last_audio_type == MSG_SPEED_UP_REQUEST);
    CHECK(fake_audio_playing);

    CHECK(message_protocol_service_publish_event(NOSTOS_FALL) == NOSTOS_OK);
    CHECK(displayed_fall);
    CHECK(audio_stop_count == 1U);
    CHECK(!fake_audio_playing);
    CHECK(message_protocol_service_publish_event(NOSTOS_FALL_CLEAR) == NOSTOS_OK);
    CHECK(!displayed_fall);
    return 0;
}

static int peer_session_controls(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    CHECK(sensor_link_encode_approve_session(1U, 100U, 4U,
        frame, &length) == SENSOR_LINK_OK);
    receive_local(frame, length, 1300U);
    tick = 1320U;
    message_protocol_service_process();

    nostos_endpoint_t *endpoint = message_protocol_service_endpoint();
    CHECK(endpoint != NULL);
    const nostos_rx_window_t *peer = &endpoint->receiver.windows[0];
    CHECK(peer->approved);
    CHECK(peer->session_id == 100U);
    CHECK(peer->floor == 4U);

    nostos_message_t request = {
        .type = NOSTOS_SPEED_DOWN,
        .source_id = 1U,
        .session_id = 100U,
        .sequence = 4U,
    };
    uint8_t nostos_frame[NOSTOS_UART_FRAME_MAX];
    size_t nostos_length = 0U;
    CHECK(encode_nostos_frame(&request, nostos_frame, &nostos_length));
    receive_local(nostos_frame, nostos_length, 1350U);
    tick = 1370U;
    message_protocol_service_process();
    CHECK(displayed_button_count == 2U);
    CHECK(displayed_sender == 1U);
    CHECK(displayed_type == NOSTOS_SPEED_DOWN);
    CHECK(fake_audio_playing);

    /* A busy BTN1/2 request is consumed and intentionally ignored. */
    request.type = NOSTOS_SPEED_UP;
    request.sequence = 5U;
    CHECK(encode_nostos_frame(&request, nostos_frame, &nostos_length));
    receive_local(nostos_frame, nostos_length, 1380U);
    tick = 1390U;
    message_protocol_service_process();
    CHECK(displayed_button_count == 2U);
    CHECK(endpoint->dropped_busy_buttons == 1U);

    /* STOP preempts the active button audio and becomes the visible request. */
    request.type = NOSTOS_STOP;
    request.sequence = 6U;
    CHECK(encode_nostos_frame(&request, nostos_frame, &nostos_length));
    receive_local(nostos_frame, nostos_length, 1395U);
    tick = 1399U;
    message_protocol_service_process();
    CHECK(displayed_button_count == 3U);
    CHECK(displayed_sender == 1U);
    CHECK(displayed_type == NOSTOS_STOP);
    CHECK(endpoint->stop_preemptions == 1U);
    CHECK(audio_stop_count == 2U);
    CHECK(last_audio_type == MSG_STOP_REQUEST);

    /* Repeating the same session is an idempotent no-op, including its floor. */
    CHECK(sensor_link_encode_approve_session(1U, 100U, 99U,
        frame, &length) == SENSOR_LINK_OK);
    receive_local(frame, length, 1400U);
    tick = 1420U;
    message_protocol_service_process();
    CHECK(peer->session_id == 100U);
    CHECK(peer->floor == 4U);

    uint32_t rejected = message_protocol_service_stats()->control_rejected;
    CHECK(sensor_link_encode_approve_session(1U, 99U, 0U,
        frame, &length) == SENSOR_LINK_OK);
    receive_local(frame, length, 1500U);
    tick = 1520U;
    message_protocol_service_process();
    CHECK(peer->session_id == 100U);
    CHECK(message_protocol_service_stats()->control_rejected == rejected + 1U);

    CHECK(sensor_link_encode_approve_session(2U, 600U, 0U,
        frame, &length) == SENSOR_LINK_OK);
    receive_local(frame, length, 1600U);
    tick = 1620U;
    message_protocol_service_process();
    CHECK(endpoint->sender.source_id == 2U);
    CHECK(endpoint->sender.session_id == 500U);
    CHECK(message_protocol_service_stats()->control_rejected == rejected + 2U);

    nostos_message_t ride = {
        .type = NOSTOS_RIDE,
        .source_id = 1U,
        .session_id = 100U,
        .sequence = 7U,
        .payload.ride = {
            .valid = true,
            .kmh_x10 = 228U,
            .distance_mm = 54321U,
        },
    };
    CHECK(encode_nostos_frame(&ride, nostos_frame, &nostos_length));
    receive_local(nostos_frame, nostos_length, 1700U);
    tick = 1720U;
    message_protocol_service_process();

    nostos_message_t environment = {
        .type = NOSTOS_ENVIRONMENT,
        .source_id = 1U,
        .session_id = 100U,
        .sequence = 8U,
        .payload.environment = {
            .temperature_c_x10 = 253,
            .humidity_pct_x10 = 610U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        },
    };
    CHECK(encode_nostos_frame(&environment, nostos_frame, &nostos_length));
    receive_local(nostos_frame, nostos_length, 1750U);
    tick = 1770U;
    message_protocol_service_process();

    /* Local ride has expired; the display view now falls back to shared_data. */
    tick = 3200U;
    sensor_view_snapshot_t view;
    CHECK(sensor_view_service_snapshot(tick, &view));
    CHECK(view.ride_source_id == 1U);
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_VALID);
    CHECK(view.sensors.ride.kmh_x10 == 228U);
    CHECK(view.sensors.ride.distance_mm == 54321U);
    CHECK(view.environment_source_id == 1U);
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_VALID);
    CHECK(view.sensors.environment.temperature_c_x10 == 253);
    CHECK(view.sensors.environment.humidity_pct_x10 == 610U);

    sensor_snapshot_t local;
    CHECK(sensor_store_snapshot(tick, &local));
    CHECK(local.ride.quality == SENSOR_QUALITY_STALE);
    CHECK(local.ride.kmh_x10 == 245U);
    CHECK(local.ride.distance_mm == 987654U);
    return 0;
}

static int identity_ack_retry(UART_HandleTypeDef *uart)
{
    transmission_count = 0U;
    transmit_failures = 0U;
    tick = 2000U;
    CHECK(message_protocol_service_boot(uart, VS1003B_STATUS_OK) == NOSTOS_OK);
    CHECK(transmission_count == 1U);

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    CHECK(sensor_link_encode_identity(3U, 700U, frame, &length) == SENSOR_LINK_OK);
    transmit_failures = 1U;
    receive_local(frame, length, 2010U);
    tick = 2030U;
    message_protocol_service_process();
    CHECK(message_protocol_service_endpoint() != NULL);
    CHECK(transmission_count == 1U);
    CHECK(message_protocol_service_stats()->identity_acks == 0U);

    tick = 2279U;
    message_protocol_service_process();
    CHECK(transmission_count == 1U);
    tick = 2280U;
    message_protocol_service_process();
    CHECK(transmission_count == 2U);
    sensor_link_message_t decoded;
    CHECK(decode_local_capture(1U, &decoded));
    CHECK(decoded.type == SENSOR_LINK_IDENTITY_ACK);
    CHECK(decoded.identity.source_id == 3U);
    CHECK(decoded.identity.session_id == 700U);
    return 0;
}

int main(void)
{
    UART_HandleTypeDef uart = {2U};
    CHECK(boot_ride_and_identity(&uart) == 0);
    CHECK(peer_session_controls() == 0);
    CHECK(identity_ack_retry(&uart) == 0);
    puts("PASS local identity handshake, pre-init ride, trusted peer sessions");
    return 0;
}
