#include "check.h"
#include "nostos_protocol.h"
#include "nostos_uart.h"

#include <stdio.h>
#include <string.h>

static void check_unchanged(const nostos_message_t *actual,
    const nostos_message_t *expected)
{
    CHECK(memcmp(actual, expected, sizeof(*actual)) == 0);
}

static size_t encode(const nostos_message_t *message, uint8_t *wire)
{
    size_t length = 0U;
    CHECK(nostos_message_encode(message, wire, NOSTOS_APPLICATION_MAX_SIZE,
        &length) == NOSTOS_OK);
    return length;
}

static void codec_golden_and_round_trips(void)
{
    nostos_message_t message;
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    nostos_message_t decoded;

    CHECK(nostos_message_make_ride(&message, 10U, true, 205U, 1234U) == NOSTOS_OK);
    const uint8_t ride_expected[] = {
        0x01, 0x0A, 0x01, 0x01, 0x01, 0xCD, 0x00,
        0xD2, 0x04, 0x00, 0x00
    };
    size_t length = encode(&message, wire);
    CHECK(length == sizeof(ride_expected));
    CHECK(memcmp(wire, ride_expected, length) == 0);
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.state_update.value.ride.speed_x10_kmh == 205U);
    CHECK(decoded.payload.state_update.value.ride.trip_distance_m == 1234U);

    CHECK(nostos_message_make_environment(&message, 2U, true, -55, 612U) == NOSTOS_OK);
    const uint8_t environment_expected[] = {
        0x01, 0x02, 0x02, 0x01, 0x01, 0xC9, 0xFF, 0x64, 0x02
    };
    length = encode(&message, wire);
    CHECK(length == sizeof(environment_expected));
    CHECK(memcmp(wire, environment_expected, length) == 0);
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.state_update.value.environment.temperature_x10_c == -55);
    CHECK(decoded.payload.state_update.value.environment.humidity_x10_pct == 612U);

    CHECK(nostos_message_make_pace(&message, 3U, 0x12345678U,
        NOSTOS_PACE_ACCELERATE) == NOSTOS_OK);
    const uint8_t pace_expected[] = {0x02,0x03,0x78,0x56,0x34,0x12,0x01};
    length = encode(&message, wire);
    CHECK(length == sizeof(pace_expected));
    CHECK(memcmp(wire, pace_expected, length) == 0);
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.pace_request.request_id == 0x12345678U);
    CHECK(decoded.payload.pace_request.action == NOSTOS_PACE_ACCELERATE);

    CHECK(nostos_message_make_stop(&message, 4U, 0xA55A00FFU,
        NOSTOS_STOP_REASON_FALL) == NOSTOS_OK);
    length = encode(&message, wire);
    CHECK(length == 7U);
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.stop_request.request_id == 0xA55A00FFU);
    CHECK(decoded.payload.stop_request.reason == NOSTOS_STOP_REASON_FALL);

    CHECK(nostos_message_make_stop_ack(&message, 5U, 0xFFFFFFFFU) == NOSTOS_OK);
    length = encode(&message, wire);
    CHECK(length == 6U);
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.stop_ack.request_id == 0xFFFFFFFFU);
}

static void invalid_sensor_normalization_and_strict_wire(void)
{
    nostos_message_t message;
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    CHECK(nostos_message_make_ride(&message, 1U, false, 999U, 999999U) == NOSTOS_OK);
    CHECK(message.payload.state_update.value.ride.speed_x10_kmh == 0U);
    CHECK(message.payload.state_update.value.ride.trip_distance_m == 0U);
    size_t length = encode(&message, wire);
    for (size_t index = 5U; index < length; ++index) CHECK(wire[index] == 0U);

    CHECK(nostos_message_make_environment(&message, 1U, false, -100, 1000U) == NOSTOS_OK);
    CHECK(message.payload.state_update.value.environment.temperature_x10_c == 0);
    CHECK(message.payload.state_update.value.environment.humidity_x10_pct == 0U);
    length = encode(&message, wire);
    for (size_t index = 5U; index < length; ++index) CHECK(wire[index] == 0U);

    wire[5] = 1U;
    nostos_message_t sentinel;
    memset(&sentinel, 0xA5, sizeof(sentinel));
    nostos_message_t unchanged = sentinel;
    CHECK(nostos_message_decode(wire, length, &sentinel) == NOSTOS_BAD_VALUE);
    check_unchanged(&sentinel, &unchanged);
}

static void codec_rejects_invalid_fields_and_lengths(void)
{
    nostos_message_t message;
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE] = {0};
    size_t length = 77U;
    CHECK(nostos_message_make_pace(&message, 0U, 1U,
        NOSTOS_PACE_ACCELERATE) == NOSTOS_OK);
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    CHECK(length == 77U);
    CHECK(nostos_message_make_pace(&message, 11U, 1U,
        NOSTOS_PACE_ACCELERATE) == NOSTOS_BAD_VALUE);
    CHECK(nostos_message_make_pace(&message, 1U, 0U,
        NOSTOS_PACE_ACCELERATE) == NOSTOS_BAD_VALUE);
    CHECK(nostos_message_make_pace(&message, 1U, 1U, 0U) == NOSTOS_BAD_VALUE);
    CHECK(nostos_message_make_stop(&message, 1U, 1U, 3U) == NOSTOS_BAD_VALUE);
    CHECK(nostos_message_make_stop_ack(&message, 11U, 1U) == NOSTOS_BAD_VALUE);

    const uint8_t invalid_source[] = {0x04,0x00,1,0,0,0};
    const uint8_t invalid_type[] = {0x05,0x01,1,0,0,0};
    const uint8_t invalid_ack_id[] = {0x04,0x01,0,0,0,0};
    const uint8_t invalid_action[] = {0x02,0x01,1,0,0,0,3};
    const uint8_t invalid_reason[] = {0x03,0x01,1,0,0,0,0};
    const uint8_t invalid_topic[] = {0x01,0x01,3,1,1,0,0,0,0};
    const uint8_t invalid_revision[] = {0x01,0x01,2,2,1,0,0,0,0};
    const uint8_t invalid_valid[] = {0x01,0x01,2,1,2,0,0,0,0};
    nostos_message_t output = {0};
    CHECK(nostos_message_decode(invalid_source, sizeof(invalid_source), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_type, sizeof(invalid_type), &output) ==
        NOSTOS_UNSUPPORTED_TYPE);
    CHECK(nostos_message_decode(invalid_ack_id, sizeof(invalid_ack_id), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_action, sizeof(invalid_action), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_reason, sizeof(invalid_reason), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_topic, sizeof(invalid_topic), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_revision, sizeof(invalid_revision), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_valid, sizeof(invalid_valid), &output) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_message_decode(invalid_action, sizeof(invalid_action) - 1U, &output) ==
        NOSTOS_BAD_LENGTH);
    CHECK(nostos_message_decode(invalid_action, NOSTOS_APPLICATION_MAX_SIZE + 1U,
        &output) == NOSTOS_TOO_LARGE);
}

static void local_source_zero_is_uart_only(void)
{
    nostos_message_t local;
    CHECK(nostos_message_make_stop(&local, NOSTOS_LOCAL_SOURCE_NODE_ID,
        0x10203040U, NOSTOS_STOP_REASON_BUTTON) == NOSTOS_OK);
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    size_t length = 999U;
    CHECK(nostos_message_encode(&local, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    CHECK(length == 999U);
    CHECK(nostos_local_message_encode(&local, wire, sizeof(wire), &length) ==
        NOSTOS_OK);
    CHECK(wire[1] == NOSTOS_LOCAL_SOURCE_NODE_ID);

    nostos_message_t decoded = {0};
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_BAD_VALUE);
    CHECK(nostos_local_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID);

    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    CHECK(nostos_uart_encode_message(&local, frame, sizeof(frame), &frame_length) ==
        NOSTOS_BAD_VALUE);
    CHECK(nostos_uart_encode_local_message(&local, frame, sizeof(frame),
        &frame_length) == NOSTOS_OK);
    nostos_uart_parser_t parser = {0};
    nostos_message_t output = {0};
    for (size_t index = 0U; index < frame_length; ++index) {
        nostos_result_t result = nostos_uart_feed_local_message(&parser,
            frame[index], (uint32_t)index, &output);
        CHECK(result == (index + 1U == frame_length ? NOSTOS_OK : NOSTOS_EMPTY));
    }
    CHECK(output.source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID);

    nostos_uart_reset(&parser);
    memset(&output, 0, sizeof(output));
    nostos_result_t final_result = NOSTOS_EMPTY;
    for (size_t index = 0U; index < frame_length; ++index) {
        final_result = nostos_uart_feed_message(&parser, frame[index],
            (uint32_t)index, &output);
    }
    CHECK(final_result == NOSTOS_BAD_VALUE);

    CHECK(nostos_message_make_stop_ack(&local,
        NOSTOS_LOCAL_SOURCE_NODE_ID, 0x55667788U) == NOSTOS_OK);
    length = 999U;
    CHECK(nostos_message_encode(&local, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    CHECK(length == 999U);
    CHECK(nostos_local_message_encode(&local, wire, sizeof(wire), &length) ==
        NOSTOS_OK);
    CHECK(wire[0] == NOSTOS_MESSAGE_STOP_ACK);
    CHECK(wire[1] == NOSTOS_LOCAL_SOURCE_NODE_ID);
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_BAD_VALUE);
    CHECK(nostos_local_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.type == NOSTOS_MESSAGE_STOP_ACK);
    CHECK(decoded.source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID);
    CHECK(decoded.payload.stop_ack.request_id == 0x55667788U);
}

static void uart_crc_fragmentation_and_embedded_magic(void)
{
    static const uint8_t check_vector[] = "123456789";
    CHECK(nostos_crc16(check_vector, sizeof(check_vector) - 1U) == 0x29B1U);

    nostos_message_t message;
    CHECK(nostos_message_make_stop(&message, 8U, 0x005AA500U,
        NOSTOS_STOP_REASON_FALL) == NOSTOS_OK);
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    CHECK(nostos_uart_encode_message(&message, frame, sizeof(frame),
        &frame_length) == NOSTOS_OK);
    CHECK(frame[0] == 0xA5U && frame[1] == 0x5AU && frame[2] == 7U);

    nostos_uart_parser_t parser = {0};
    nostos_message_t decoded = {0};
    for (size_t index = 0U; index < frame_length; ++index) {
        nostos_result_t result = nostos_uart_feed_message(&parser, frame[index],
            (uint32_t)(index * 3U), &decoded);
        CHECK(result == (index + 1U == frame_length ? NOSTOS_OK : NOSTOS_EMPTY));
    }
    CHECK(decoded.payload.stop_request.request_id == 0x005AA500U);
}

static void uart_crc_error_resync_and_timeout(void)
{
    nostos_message_t message;
    CHECK(nostos_message_make_stop_ack(&message, 2U, 0xABCDEF01U) == NOSTOS_OK);
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    CHECK(nostos_uart_encode_message(&message, frame, sizeof(frame),
        &frame_length) == NOSTOS_OK);

    uint8_t damaged[NOSTOS_UART_FRAME_MAX];
    memcpy(damaged, frame, frame_length);
    damaged[4] ^= 0x80U;
    nostos_uart_parser_t parser = {0};
    nostos_message_t output;
    memset(&output, 0x5A, sizeof(output));
    nostos_message_t unchanged = output;
    for (size_t index = 0U; index < frame_length; ++index) {
        nostos_result_t result = nostos_uart_feed_message(&parser, damaged[index],
            (uint32_t)index, &output);
        CHECK(result == (index + 1U == frame_length ? NOSTOS_BAD_CRC : NOSTOS_EMPTY));
    }
    check_unchanged(&output, &unchanged);

    const uint8_t junk[] = {0x00,0xA5,0x11,0xA5};
    for (size_t index = 0U; index < sizeof(junk); ++index) {
        CHECK(nostos_uart_feed_message(&parser, junk[index],
            (uint32_t)(100U + index), &output) == NOSTOS_EMPTY);
    }
    /* Parser already holds A5, so the next frame's A5 keeps it synchronized. */
    for (size_t index = 0U; index < frame_length; ++index) {
        nostos_result_t result = nostos_uart_feed_message(&parser, frame[index],
            (uint32_t)(110U + index), &output);
        CHECK(result == (index + 1U == frame_length ? NOSTOS_OK : NOSTOS_EMPTY));
    }
    CHECK(output.type == NOSTOS_MESSAGE_STOP_ACK);
    CHECK(output.payload.stop_ack.request_id == 0xABCDEF01U);

    nostos_uart_reset(&parser);
    CHECK(nostos_uart_feed_message(&parser, 0xA5U, 1000U, &output) == NOSTOS_EMPTY);
    CHECK(nostos_uart_feed_message(&parser, 0x5AU, 1001U, &output) == NOSTOS_EMPTY);
    CHECK(nostos_uart_feed_message(&parser, 6U, 1200U, &output) == NOSTOS_TIMEOUT);
    CHECK(nostos_uart_feed_message(&parser, frame[0], 1201U, &output) == NOSTOS_EMPTY);
    for (size_t index = 1U; index < frame_length; ++index) {
        nostos_result_t result = nostos_uart_feed_message(&parser, frame[index],
            (uint32_t)(1201U + index), &output);
        CHECK(result == (index + 1U == frame_length ? NOSTOS_OK : NOSTOS_EMPTY));
    }
}

static void uart_false_preamble_preserves_next_magic(void)
{
    nostos_message_t message;
    CHECK(nostos_message_make_stop_ack(
        &message, 2U, 0x10203040U) == NOSTOS_OK);
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    CHECK(nostos_uart_encode_message(
        &message, frame, sizeof(frame), &frame_length) == NOSTOS_OK);

    nostos_uart_parser_t parser = {0};
    nostos_message_t output = {0};
    CHECK(nostos_uart_feed_message(
        &parser, NOSTOS_UART_MAGIC_0, 0U, &output) == NOSTOS_EMPTY);
    CHECK(nostos_uart_feed_message(
        &parser, NOSTOS_UART_MAGIC_1, 1U, &output) == NOSTOS_EMPTY);
    /* This byte is both the invalid length and the real frame's first magic. */
    CHECK(nostos_uart_feed_message(
        &parser, frame[0], 2U, &output) == NOSTOS_TOO_LARGE);
    for (size_t index = 1U; index < frame_length; ++index) {
        nostos_result_t result = nostos_uart_feed_message(
            &parser, frame[index], (uint32_t)(2U + index), &output);
        CHECK(result ==
            (index + 1U == frame_length ? NOSTOS_OK : NOSTOS_EMPTY));
    }
    CHECK(output.type == NOSTOS_MESSAGE_STOP_ACK);
    CHECK(output.payload.stop_ack.request_id == 0x10203040U);
}

int main(void)
{
    codec_golden_and_round_trips();
    invalid_sensor_normalization_and_strict_wire();
    codec_rejects_invalid_fields_and_lengths();
    local_source_zero_is_uart_only();
    uart_crc_fragmentation_and_embedded_magic();
    uart_crc_error_resync_and_timeout();
    uart_false_preamble_preserves_next_magic();
    puts("application protocol tests: OK");
    return 0;
}
