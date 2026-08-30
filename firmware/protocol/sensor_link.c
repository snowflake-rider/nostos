#include "sensor_link.h"

#include <string.h>

#define SENSOR_LINK_HEADER_SIZE 5U
#define SENSOR_LINK_CRC_SIZE 2U
#define SENSOR_LINK_MAX_PAYLOAD_SIZE SENSOR_LINK_OUTPUT_RIDE_PAYLOAD_SIZE

enum {
    LOCAL_SHARED_DATA_RIDE = 1U << 0,
    LOCAL_SHARED_DATA_ENVIRONMENT = 1U << 1,
    LOCAL_SHARED_DATA_MASK =
        LOCAL_SHARED_DATA_RIDE | LOCAL_SHARED_DATA_ENVIRONMENT,
};

static uint16_t crc16(const uint8_t *bytes, size_t length)
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

static bool valid_identity(uint8_t source_id, uint32_t session_id)
{
    return source_id >= SENSOR_LINK_SOURCE_ID_MIN &&
        source_id <= SENSOR_LINK_SOURCE_ID_MAX && session_id != 0U;
}

static bool valid_event(uint8_t type)
{
    return type == SENSOR_LINK_EVENT_SPEED_DOWN ||
        type == SENSOR_LINK_EVENT_SPEED_UP ||
        type == SENSOR_LINK_EVENT_STOP || type == SENSOR_LINK_EVENT_FALL ||
        type == SENSOR_LINK_EVENT_FALL_CLEAR;
}

static bool valid_quality(uint8_t quality)
{
    return quality <= SENSOR_LINK_QUALITY_MAX;
}

static bool valid_source_id(uint8_t source_id)
{
    return source_id >= SENSOR_LINK_SOURCE_ID_MIN &&
        source_id <= SENSOR_LINK_SOURCE_ID_MAX;
}

static bool valid_output_target(uint32_t command_id, uint8_t source_id)
{
    return command_id != 0U && valid_source_id(source_id);
}

static bool valid_output_status(uint8_t status)
{
    return status <= SENSOR_LINK_OUTPUT_HARDWARE_ERROR;
}

static bool valid_shared_data_mask(uint8_t mask)
{
    return mask != 0U && (mask & (uint8_t)~LOCAL_SHARED_DATA_MASK) == 0U;
}

static sensor_link_result_t encode_frame(
    uint8_t type,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL ||
        (payload_length != 0U && payload == NULL)) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (payload_length > SENSOR_LINK_MAX_PAYLOAD_SIZE) {
        return SENSOR_LINK_BAD_LENGTH;
    }

    uint8_t encoded[SENSOR_LINK_FRAME_SIZE] = {0};
    encoded[0] = SENSOR_LINK_PREAMBLE_0;
    encoded[1] = SENSOR_LINK_PREAMBLE_1;
    encoded[2] = SENSOR_LINK_VERSION;
    encoded[3] = type;
    encoded[4] = (uint8_t)payload_length;
    if (payload_length != 0U) {
        memcpy(&encoded[SENSOR_LINK_HEADER_SIZE], payload, payload_length);
    }

    size_t crc_index = SENSOR_LINK_HEADER_SIZE + payload_length;
    uint16_t checksum = crc16(&encoded[2], 3U + payload_length);
    encoded[crc_index] = (uint8_t)checksum;
    encoded[crc_index + 1U] = (uint8_t)(checksum >> 8);

    size_t encoded_length = crc_index + SENSOR_LINK_CRC_SIZE;
    memcpy(frame, encoded, encoded_length);
    *frame_length = encoded_length;
    return SENSOR_LINK_OK;
}

sensor_link_result_t sensor_link_encode_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid && (kmh_x10 != 0U || distance_mm != 0U)) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint8_t payload[SENSOR_LINK_RIDE_PAYLOAD_SIZE] = {
        valid ? 1U : 0U,
        (uint8_t)kmh_x10,
        (uint8_t)(kmh_x10 >> 8),
        (uint8_t)distance_mm,
        (uint8_t)(distance_mm >> 8),
        (uint8_t)(distance_mm >> 16),
        (uint8_t)(distance_mm >> 24),
    };
    return encode_frame(SENSOR_LINK_RIDE, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_event(
    uint8_t event_type,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_event(event_type)) {
        return SENSOR_LINK_BAD_VALUE;
    }
    const uint8_t payload[SENSOR_LINK_EVENT_PAYLOAD_SIZE] = {event_type};
    return encode_frame(SENSOR_LINK_EVENT, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_quality(temperature_quality) ||
        !valid_quality(humidity_quality)) {
        return SENSOR_LINK_BAD_VALUE;
    }
    const uint16_t temperature_bits = (uint16_t)temperature_c_x10;
    const uint8_t payload[SENSOR_LINK_ENVIRONMENT_PAYLOAD_SIZE] = {
        (uint8_t)temperature_bits,
        (uint8_t)(temperature_bits >> 8),
        (uint8_t)humidity_pct_x10,
        (uint8_t)(humidity_pct_x10 >> 8),
        temperature_quality,
        humidity_quality,
    };
    return encode_frame(
        SENSOR_LINK_ENVIRONMENT, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_shared_data_request(
    uint8_t mask,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_shared_data_mask(mask)) {
        return SENSOR_LINK_BAD_VALUE;
    }
    const uint8_t payload[SENSOR_LINK_SHARED_DATA_REQUEST_PAYLOAD_SIZE] = {mask};
    return encode_frame(
        SENSOR_LINK_SHARED_DATA_REQUEST, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_ready(
    uint32_t command_epoch,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (command_epoch == 0U) {
        return SENSOR_LINK_BAD_VALUE;
    }
    const uint8_t payload[SENSOR_LINK_READY_PAYLOAD_SIZE] = {
        (uint8_t)command_epoch,
        (uint8_t)(command_epoch >> 8),
        (uint8_t)(command_epoch >> 16),
        (uint8_t)(command_epoch >> 24),
    };
    return encode_frame(
        SENSOR_LINK_READY, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_output_event(
    uint32_t command_id,
    uint8_t source_id,
    uint8_t event_type,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_output_target(command_id, source_id) ||
        !valid_event(event_type)) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint8_t payload[SENSOR_LINK_OUTPUT_EVENT_PAYLOAD_SIZE] = {
        (uint8_t)command_id,
        (uint8_t)(command_id >> 8),
        (uint8_t)(command_id >> 16),
        (uint8_t)(command_id >> 24),
        source_id,
        event_type,
    };
    return encode_frame(
        SENSOR_LINK_OUTPUT_EVENT, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_output_ride(
    uint32_t command_id,
    uint8_t source_id,
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_output_target(command_id, source_id) ||
        (!valid && (kmh_x10 != 0U || distance_mm != 0U))) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint8_t payload[SENSOR_LINK_OUTPUT_RIDE_PAYLOAD_SIZE] = {
        (uint8_t)command_id,
        (uint8_t)(command_id >> 8),
        (uint8_t)(command_id >> 16),
        (uint8_t)(command_id >> 24),
        source_id,
        valid ? 1U : 0U,
        (uint8_t)kmh_x10,
        (uint8_t)(kmh_x10 >> 8),
        (uint8_t)distance_mm,
        (uint8_t)(distance_mm >> 8),
        (uint8_t)(distance_mm >> 16),
        (uint8_t)(distance_mm >> 24),
    };
    return encode_frame(
        SENSOR_LINK_OUTPUT_RIDE, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_output_environment(
    uint32_t command_id,
    uint8_t source_id,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_output_target(command_id, source_id) ||
        !valid_quality(temperature_quality) ||
        !valid_quality(humidity_quality)) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint16_t temperature_bits = (uint16_t)temperature_c_x10;
    const uint8_t payload[SENSOR_LINK_OUTPUT_ENVIRONMENT_PAYLOAD_SIZE] = {
        (uint8_t)command_id,
        (uint8_t)(command_id >> 8),
        (uint8_t)(command_id >> 16),
        (uint8_t)(command_id >> 24),
        source_id,
        (uint8_t)temperature_bits,
        (uint8_t)(temperature_bits >> 8),
        (uint8_t)humidity_pct_x10,
        (uint8_t)(humidity_pct_x10 >> 8),
        temperature_quality,
        humidity_quality,
    };
    return encode_frame(SENSOR_LINK_OUTPUT_ENVIRONMENT, payload,
        sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_output_result(
    uint32_t command_id,
    uint8_t status,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (command_id == 0U || !valid_output_status(status)) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint8_t payload[SENSOR_LINK_OUTPUT_RESULT_PAYLOAD_SIZE] = {
        (uint8_t)command_id,
        (uint8_t)(command_id >> 8),
        (uint8_t)(command_id >> 16),
        (uint8_t)(command_id >> 24),
        status,
    };
    return encode_frame(
        SENSOR_LINK_OUTPUT_RESULT, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_hello(
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    return encode_frame(SENSOR_LINK_HELLO, NULL, 0U, frame, frame_length);
}

static sensor_link_result_t encode_identity_frame(
    uint8_t type,
    uint8_t source_id,
    uint32_t session_id,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_identity(source_id, session_id)) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint8_t payload[SENSOR_LINK_IDENTITY_PAYLOAD_SIZE] = {
        source_id,
        (uint8_t)session_id,
        (uint8_t)(session_id >> 8),
        (uint8_t)(session_id >> 16),
        (uint8_t)(session_id >> 24),
    };
    return encode_frame(type, payload, sizeof(payload), frame, frame_length);
}

sensor_link_result_t sensor_link_encode_identity(
    uint8_t source_id,
    uint32_t session_id,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    return encode_identity_frame(
        SENSOR_LINK_IDENTITY, source_id, session_id, frame, frame_length);
}

sensor_link_result_t sensor_link_encode_identity_ack(
    uint8_t source_id,
    uint32_t session_id,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    return encode_identity_frame(
        SENSOR_LINK_IDENTITY_ACK, source_id, session_id, frame, frame_length);
}

sensor_link_result_t sensor_link_encode_approve_session(
    uint8_t source_id,
    uint32_t session_id,
    uint16_t sequence_floor,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }
    if (!valid_identity(source_id, session_id)) {
        return SENSOR_LINK_BAD_VALUE;
    }

    const uint8_t payload[SENSOR_LINK_APPROVE_SESSION_PAYLOAD_SIZE] = {
        source_id,
        (uint8_t)session_id,
        (uint8_t)(session_id >> 8),
        (uint8_t)(session_id >> 16),
        (uint8_t)(session_id >> 24),
        (uint8_t)sequence_floor,
        (uint8_t)(sequence_floor >> 8),
    };
    return encode_frame(
        SENSOR_LINK_APPROVE_SESSION, payload, sizeof(payload), frame, frame_length);
}

void sensor_link_reset(sensor_link_parser_t *parser)
{
    if (parser != NULL) {
        *parser = (sensor_link_parser_t){0};
    }
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
        (uint16_t)((uint16_t)bytes[1] << 8));
}

static sensor_link_result_t decode_complete(
    const sensor_link_parser_t *parser,
    sensor_link_message_t *message)
{
    const uint8_t *frame = parser->bytes;
    size_t payload_length = frame[4];
    size_t crc_index = SENSOR_LINK_HEADER_SIZE + payload_length;
    uint16_t expected = crc16(&frame[2], 3U + payload_length);
    uint16_t received = (uint16_t)((uint16_t)frame[crc_index] |
        (uint16_t)((uint16_t)frame[crc_index + 1U] << 8));
    if (received != expected) {
        return SENSOR_LINK_BAD_CRC;
    }
    if (frame[2] != SENSOR_LINK_VERSION) {
        return SENSOR_LINK_BAD_VERSION;
    }

    sensor_link_message_t decoded = {.type = frame[3]};
    const uint8_t *payload = &frame[SENSOR_LINK_HEADER_SIZE];
    switch (frame[3]) {
    case SENSOR_LINK_RIDE: {
        if (payload_length != SENSOR_LINK_RIDE_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        if (payload[0] > 1U) {
            return SENSOR_LINK_BAD_VALUE;
        }
        uint16_t kmh_x10 = (uint16_t)((uint16_t)payload[1] |
            (uint16_t)((uint16_t)payload[2] << 8));
        uint32_t distance_mm = read_u32_le(&payload[3]);
        if (payload[0] == 0U && (kmh_x10 != 0U || distance_mm != 0U)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        decoded.ride.valid = payload[0] != 0U;
        decoded.ride.kmh_x10 = kmh_x10;
        decoded.ride.distance_mm = distance_mm;
        break;
    }
    case SENSOR_LINK_EVENT:
        if (payload_length != SENSOR_LINK_EVENT_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        if (!valid_event(payload[0])) {
            return SENSOR_LINK_BAD_VALUE;
        }
        decoded.event.type = payload[0];
        break;
    case SENSOR_LINK_ENVIRONMENT:
        if (payload_length != SENSOR_LINK_ENVIRONMENT_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.environment.temperature_c_x10 =
            (int16_t)read_u16_le(&payload[0]);
        decoded.environment.humidity_pct_x10 = read_u16_le(&payload[2]);
        decoded.environment.temperature_quality = payload[4];
        decoded.environment.humidity_quality = payload[5];
        if (!valid_quality(decoded.environment.temperature_quality) ||
            !valid_quality(decoded.environment.humidity_quality)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_SHARED_DATA_REQUEST:
        if (payload_length != SENSOR_LINK_SHARED_DATA_REQUEST_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        if (!valid_shared_data_mask(payload[0])) {
            return SENSOR_LINK_BAD_VALUE;
        }
        decoded.shared_data_request.mask = payload[0];
        break;
    case SENSOR_LINK_READY:
        if (payload_length != SENSOR_LINK_READY_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.ready.command_epoch = read_u32_le(&payload[0]);
        if (decoded.ready.command_epoch == 0U) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_OUTPUT_EVENT:
        if (payload_length != SENSOR_LINK_OUTPUT_EVENT_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.output_event.command_id = read_u32_le(&payload[0]);
        decoded.output_event.source_id = payload[4];
        decoded.output_event.event_type = payload[5];
        if (!valid_output_target(decoded.output_event.command_id,
                decoded.output_event.source_id) ||
            !valid_event(decoded.output_event.event_type)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_OUTPUT_RIDE:
        if (payload_length != SENSOR_LINK_OUTPUT_RIDE_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.output_ride.command_id = read_u32_le(&payload[0]);
        decoded.output_ride.source_id = payload[4];
        if (!valid_output_target(decoded.output_ride.command_id,
                decoded.output_ride.source_id) || payload[5] > 1U) {
            return SENSOR_LINK_BAD_VALUE;
        }
        decoded.output_ride.valid = payload[5] != 0U;
        decoded.output_ride.kmh_x10 = read_u16_le(&payload[6]);
        decoded.output_ride.distance_mm = read_u32_le(&payload[8]);
        if (!decoded.output_ride.valid &&
            (decoded.output_ride.kmh_x10 != 0U ||
                decoded.output_ride.distance_mm != 0U)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_OUTPUT_ENVIRONMENT:
        if (payload_length != SENSOR_LINK_OUTPUT_ENVIRONMENT_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.output_environment.command_id = read_u32_le(&payload[0]);
        decoded.output_environment.source_id = payload[4];
        decoded.output_environment.temperature_c_x10 =
            (int16_t)read_u16_le(&payload[5]);
        decoded.output_environment.humidity_pct_x10 = read_u16_le(&payload[7]);
        decoded.output_environment.temperature_quality = payload[9];
        decoded.output_environment.humidity_quality = payload[10];
        if (!valid_output_target(decoded.output_environment.command_id,
                decoded.output_environment.source_id) ||
            !valid_quality(decoded.output_environment.temperature_quality) ||
            !valid_quality(decoded.output_environment.humidity_quality)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_OUTPUT_RESULT:
        if (payload_length != SENSOR_LINK_OUTPUT_RESULT_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.output_result.command_id = read_u32_le(&payload[0]);
        decoded.output_result.status = payload[4];
        if (decoded.output_result.command_id == 0U ||
            !valid_output_status(decoded.output_result.status)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_HELLO:
        if (payload_length != SENSOR_LINK_HELLO_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        break;
    case SENSOR_LINK_IDENTITY:
    case SENSOR_LINK_IDENTITY_ACK:
        if (payload_length != SENSOR_LINK_IDENTITY_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.identity.source_id = payload[0];
        decoded.identity.session_id = read_u32_le(&payload[1]);
        if (!valid_identity(decoded.identity.source_id, decoded.identity.session_id)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    case SENSOR_LINK_APPROVE_SESSION:
        if (payload_length != SENSOR_LINK_APPROVE_SESSION_PAYLOAD_SIZE) {
            return SENSOR_LINK_BAD_LENGTH;
        }
        decoded.approve_session.source_id = payload[0];
        decoded.approve_session.session_id = read_u32_le(&payload[1]);
        decoded.approve_session.sequence_floor = (uint16_t)((uint16_t)payload[5] |
            (uint16_t)((uint16_t)payload[6] << 8));
        if (!valid_identity(decoded.approve_session.source_id,
                decoded.approve_session.session_id)) {
            return SENSOR_LINK_BAD_VALUE;
        }
        break;
    default:
        return SENSOR_LINK_BAD_TYPE;
    }

    *message = decoded;
    return SENSOR_LINK_OK;
}

sensor_link_result_t sensor_link_feed(
    sensor_link_parser_t *parser,
    uint8_t byte,
    uint32_t now_ms,
    sensor_link_message_t *message)
{
    if (parser == NULL || message == NULL) {
        return SENSOR_LINK_BAD_ARGUMENT;
    }

    bool timed_out = parser->used != 0U &&
        (uint32_t)(now_ms - parser->last_byte_ms) > SENSOR_LINK_TIMEOUT_MS;
    if (timed_out) {
        sensor_link_reset(parser);
    }

    if (parser->used == 0U) {
        if (byte == SENSOR_LINK_PREAMBLE_0) {
            parser->bytes[0] = byte;
            parser->used = 1U;
            parser->last_byte_ms = now_ms;
            return SENSOR_LINK_EMPTY;
        }
        return timed_out ? SENSOR_LINK_TIMEOUT : SENSOR_LINK_EMPTY;
    }

    if (parser->used == 1U && byte != SENSOR_LINK_PREAMBLE_1) {
        if (byte == SENSOR_LINK_PREAMBLE_0) {
            parser->bytes[0] = byte;
            parser->used = 1U;
            parser->last_byte_ms = now_ms;
            return SENSOR_LINK_EMPTY;
        }
        parser->used = 0U;
        parser->last_byte_ms = now_ms;
        return SENSOR_LINK_BAD_VALUE;
    }

    if (parser->used >= SENSOR_LINK_FRAME_SIZE) {
        sensor_link_reset(parser);
        return SENSOR_LINK_BAD_LENGTH;
    }
    parser->bytes[parser->used++] = byte;
    parser->last_byte_ms = now_ms;

    if (parser->used == SENSOR_LINK_HEADER_SIZE &&
        parser->bytes[4] > SENSOR_LINK_MAX_PAYLOAD_SIZE) {
        sensor_link_reset(parser);
        return SENSOR_LINK_BAD_LENGTH;
    }
    if (parser->used < SENSOR_LINK_HEADER_SIZE) {
        return SENSOR_LINK_EMPTY;
    }

    size_t expected_length = SENSOR_LINK_FRAME_OVERHEAD_SIZE + parser->bytes[4];
    if (parser->used < expected_length) {
        return SENSOR_LINK_EMPTY;
    }
    if (parser->used != expected_length) {
        sensor_link_reset(parser);
        return SENSOR_LINK_BAD_LENGTH;
    }

    sensor_link_result_t result = decode_complete(parser, message);
    sensor_link_reset(parser);
    return result;
}

const char *sensor_link_result_name(sensor_link_result_t result)
{
    static const char *const names[] = {
        "OK", "EMPTY", "BAD_ARGUMENT", "BAD_VERSION", "BAD_TYPE",
        "BAD_LENGTH", "BAD_VALUE", "BAD_CRC", "TIMEOUT"
    };
    size_t count = sizeof(names) / sizeof(names[0]);
    return (size_t)result < count ? names[result] : "UNKNOWN";
}
