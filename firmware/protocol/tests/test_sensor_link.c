#include "check.h"
#include "sensor_link.h"

#include <string.h>

static uint16_t fixture_crc16(const uint8_t *bytes, size_t length)
{
    uint16_t crc = 0xFFFFU;
    for (size_t index = 0U; index < length; ++index) {
        crc ^= (uint16_t)((uint16_t)bytes[index] << 8);
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (uint16_t)((uint16_t)(crc << 1) ^
                ((crc & 0x8000U) != 0U ? 0x1021U : 0U));
        }
    }
    return crc;
}

static void refresh_fixture_crc(uint8_t *frame)
{
    size_t payload_length = frame[4];
    size_t crc_index = 5U + payload_length;
    uint16_t crc = fixture_crc16(&frame[2], 3U + payload_length);
    frame[crc_index] = (uint8_t)crc;
    frame[crc_index + 1U] = (uint8_t)(crc >> 8);
}

static sensor_link_result_t feed_frame(
    sensor_link_parser_t *parser,
    const uint8_t *frame,
    size_t length,
    uint32_t start_ms,
    sensor_link_message_t *message)
{
    sensor_link_result_t result = SENSOR_LINK_EMPTY;
    for (size_t index = 0U; index < length; ++index) {
        result = sensor_link_feed(
            parser, frame[index], start_ms + (uint32_t)index, message);
        if (index + 1U < length) {
            CHECK(result == SENSOR_LINK_EMPTY);
        }
    }
    return result;
}

static void round_trip_ride(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    CHECK(sensor_link_encode_ride(
        true, 228U, UINT32_C(0x11223344), frame, &length) == SENSOR_LINK_OK);
    CHECK(length == SENSOR_LINK_RIDE_FRAME_SIZE);
    CHECK(frame[0] == SENSOR_LINK_PREAMBLE_0);
    CHECK(frame[1] == SENSOR_LINK_PREAMBLE_1);
    CHECK(frame[2] == SENSOR_LINK_VERSION);
    CHECK(frame[3] == SENSOR_LINK_RIDE);
    CHECK(frame[4] == SENSOR_LINK_RIDE_PAYLOAD_SIZE);
    CHECK(frame[5] == 1U);
    CHECK(frame[6] == 0xE4U);
    CHECK(frame[7] == 0x00U);
    CHECK(frame[8] == 0x44U);
    CHECK(frame[9] == 0x33U);
    CHECK(frame[10] == 0x22U);
    CHECK(frame[11] == 0x11U);
    CHECK(frame[12] == 0x18U);
    CHECK(frame[13] == 0x7CU);

    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {0};
    CHECK(feed_frame(&parser, frame, length, 1U, &message) == SENSOR_LINK_OK);
    CHECK(message.type == SENSOR_LINK_RIDE);
    CHECK(message.ride.valid);
    CHECK(message.ride.kmh_x10 == 228U);
    CHECK(message.ride.distance_mm == UINT32_C(0x11223344));
}

static void round_trip_handshake(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {0};

    CHECK(sensor_link_encode_hello(frame, &length) == SENSOR_LINK_OK);
    CHECK(length == SENSOR_LINK_HELLO_FRAME_SIZE);
    CHECK(frame[3] == SENSOR_LINK_HELLO);
    CHECK(frame[4] == 0U);
    CHECK(frame[5] == 0xCEU);
    CHECK(frame[6] == 0x9DU);
    CHECK(feed_frame(&parser, frame, length, 1U, &message) == SENSOR_LINK_OK);
    CHECK(message.type == SENSOR_LINK_HELLO);

    CHECK(sensor_link_encode_identity(2U, 0x78563412U, frame, &length) ==
        SENSOR_LINK_OK);
    CHECK(length == SENSOR_LINK_IDENTITY_FRAME_SIZE);
    CHECK(frame[3] == SENSOR_LINK_IDENTITY);
    CHECK(frame[5] == 2U);
    CHECK(frame[6] == 0x12U);
    CHECK(frame[7] == 0x34U);
    CHECK(frame[8] == 0x56U);
    CHECK(frame[9] == 0x78U);
    CHECK(frame[10] == 0xC1U);
    CHECK(frame[11] == 0x1DU);
    CHECK(feed_frame(&parser, frame, length, 20U, &message) == SENSOR_LINK_OK);
    CHECK(message.type == SENSOR_LINK_IDENTITY);
    CHECK(message.identity.source_id == 2U);
    CHECK(message.identity.session_id == 0x78563412U);

    CHECK(sensor_link_encode_approve_session(
        3U, 0xA1B2C3D4U, 0x5678U, frame, &length) == SENSOR_LINK_OK);
    CHECK(length == SENSOR_LINK_APPROVE_SESSION_FRAME_SIZE);
    CHECK(length == SENSOR_LINK_FRAME_SIZE);
    CHECK(frame[3] == SENSOR_LINK_APPROVE_SESSION);
    CHECK(frame[10] == 0x78U);
    CHECK(frame[11] == 0x56U);
    CHECK(frame[12] == 0xA0U);
    CHECK(frame[13] == 0x5FU);
    CHECK(feed_frame(&parser, frame, length, 40U, &message) == SENSOR_LINK_OK);
    CHECK(message.type == SENSOR_LINK_APPROVE_SESSION);
    CHECK(message.approve_session.source_id == 3U);
    CHECK(message.approve_session.session_id == 0xA1B2C3D4U);
    CHECK(message.approve_session.sequence_floor == 0x5678U);

    CHECK(sensor_link_encode_identity_ack(1U, 0x10203040U, frame, &length) ==
        SENSOR_LINK_OK);
    CHECK(length == SENSOR_LINK_IDENTITY_ACK_FRAME_SIZE);
    CHECK(frame[3] == SENSOR_LINK_IDENTITY_ACK);
    CHECK(frame[10] == 0xF4U);
    CHECK(frame[11] == 0x59U);
    CHECK(feed_frame(&parser, frame, length, 60U, &message) == SENSOR_LINK_OK);
    CHECK(message.type == SENSOR_LINK_IDENTITY_ACK);
    CHECK(message.identity.source_id == 1U);
    CHECK(message.identity.session_id == 0x10203040U);
}

static void invalid_encode_preserves_outputs(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    uint8_t original[SENSOR_LINK_FRAME_SIZE];
    memset(frame, 0xA7, sizeof(frame));
    memcpy(original, frame, sizeof(original));
    size_t length = 99U;

    CHECK(sensor_link_encode_ride(false, 1U, 0U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(sensor_link_encode_ride(false, 0U, 1U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(length == 99U);
    CHECK(memcmp(frame, original, sizeof(frame)) == 0);
    CHECK(sensor_link_encode_identity(0U, 1U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(sensor_link_encode_identity(4U, 1U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(sensor_link_encode_identity(1U, 0U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(sensor_link_encode_approve_session(0U, 1U, 0U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(sensor_link_encode_identity_ack(1U, 0U, frame, &length) ==
        SENSOR_LINK_BAD_VALUE);
    CHECK(length == 99U);
    CHECK(memcmp(frame, original, sizeof(frame)) == 0);

    CHECK(sensor_link_encode_hello(NULL, &length) == SENSOR_LINK_BAD_ARGUMENT);
    CHECK(sensor_link_encode_hello(frame, NULL) == SENSOR_LINK_BAD_ARGUMENT);
}

static void crc_error_and_recovery(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    CHECK(sensor_link_encode_ride(false, 0U, 0U, frame, &length) == SENSOR_LINK_OK);

    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {.type = 0xEEU};
    frame[length - 2U] ^= 0x01U;
    CHECK(feed_frame(&parser, frame, length, 1U, &message) == SENSOR_LINK_BAD_CRC);
    CHECK(message.type == 0xEEU);
    CHECK(parser.used == 0U);

    CHECK(sensor_link_encode_ride(true, 315U, 777U, frame, &length) == SENSOR_LINK_OK);
    CHECK(feed_frame(&parser, frame, length, 100U, &message) == SENSOR_LINK_OK);
    CHECK(message.ride.kmh_x10 == 315U);
    CHECK(message.ride.distance_mm == 777U);

    frame[3] = 0x01U; /* Retired local SPEED type must not revive. */
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 200U, &message) == SENSOR_LINK_BAD_TYPE);
}

static void decoded_identity_values_are_validated(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {.type = 0xEEU};

    CHECK(sensor_link_encode_identity(2U, 0x12345678U, frame, &length) ==
        SENSOR_LINK_OK);
    frame[5] = 0U;
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 1U, &message) == SENSOR_LINK_BAD_VALUE);
    CHECK(message.type == 0xEEU);

    CHECK(sensor_link_encode_identity(2U, 0x12345678U, frame, &length) ==
        SENSOR_LINK_OK);
    memset(&frame[6], 0, sizeof(uint32_t));
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 30U, &message) == SENSOR_LINK_BAD_VALUE);
    CHECK(message.type == 0xEEU);

    CHECK(sensor_link_encode_approve_session(
        3U, 0x12345678U, 0U, frame, &length) == SENSOR_LINK_OK);
    frame[5] = 4U;
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 60U, &message) == SENSOR_LINK_BAD_VALUE);
    CHECK(message.type == 0xEEU);
}

static void decoded_ride_values_are_validated(void)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {.type = 0xEEU};

    CHECK(sensor_link_encode_ride(true, 10U, 20U, frame, &length) == SENSOR_LINK_OK);
    frame[5] = 0U;
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 1U, &message) == SENSOR_LINK_BAD_VALUE);
    CHECK(message.type == 0xEEU);

    CHECK(sensor_link_encode_ride(true, 0U, 20U, frame, &length) == SENSOR_LINK_OK);
    frame[5] = 0U;
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 30U, &message) == SENSOR_LINK_BAD_VALUE);
    CHECK(message.type == 0xEEU);

    CHECK(sensor_link_encode_ride(true, 0U, 0U, frame, &length) == SENSOR_LINK_OK);
    frame[5] = 2U;
    refresh_fixture_crc(frame);
    CHECK(feed_frame(&parser, frame, length, 60U, &message) == SENSOR_LINK_BAD_VALUE);
    CHECK(message.type == 0xEEU);
}

static void length_timeout_and_preamble_resync(void)
{
    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {0};

    CHECK(sensor_link_feed(&parser, SENSOR_LINK_PREAMBLE_0, 1U, &message) ==
        SENSOR_LINK_EMPTY);
    CHECK(sensor_link_feed(&parser, SENSOR_LINK_PREAMBLE_0, 2U, &message) ==
        SENSOR_LINK_EMPTY);
    CHECK(parser.used == 1U);

    uint8_t hello[SENSOR_LINK_FRAME_SIZE];
    size_t hello_length = 0U;
    CHECK(sensor_link_encode_hello(hello, &hello_length) == SENSOR_LINK_OK);
    CHECK(feed_frame(&parser, &hello[1], hello_length - 1U, 3U, &message) ==
        SENSOR_LINK_OK);
    CHECK(message.type == SENSOR_LINK_HELLO);

    CHECK(sensor_link_feed(&parser, SENSOR_LINK_PREAMBLE_0, 20U, &message) ==
        SENSOR_LINK_EMPTY);
    CHECK(sensor_link_feed(&parser, SENSOR_LINK_PREAMBLE_1,
        20U + SENSOR_LINK_TIMEOUT_MS + 1U, &message) == SENSOR_LINK_TIMEOUT);
    CHECK(parser.used == 0U);

    const uint8_t bad_length_prefix[] = {
        SENSOR_LINK_PREAMBLE_0, SENSOR_LINK_PREAMBLE_1,
        SENSOR_LINK_VERSION, SENSOR_LINK_HELLO, 8U,
    };
    sensor_link_result_t result = SENSOR_LINK_EMPTY;
    for (size_t index = 0U; index < sizeof(bad_length_prefix); ++index) {
        result = sensor_link_feed(
            &parser, bad_length_prefix[index], 200U + (uint32_t)index, &message);
    }
    CHECK(result == SENSOR_LINK_BAD_LENGTH);
    CHECK(parser.used == 0U);

    CHECK(feed_frame(&parser, hello, hello_length, 300U, &message) == SENSOR_LINK_OK);
}

int main(void)
{
    round_trip_ride();
    round_trip_handshake();
    invalid_encode_preserves_outputs();
    crc_error_and_recovery();
    decoded_identity_values_are_validated();
    decoded_ride_values_are_validated();
    length_timeout_and_preamble_resync();
    return 0;
}
