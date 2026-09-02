#include "nostos_protocol.h"

#include <string.h>

static void put_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

static void put_u32_le(uint8_t *bytes, uint32_t value)
{
    put_u16_le(bytes, (uint16_t)value);
    put_u16_le(bytes + 2, (uint16_t)(value >> 16U));
}

static uint16_t get_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
        (uint16_t)((uint16_t)bytes[1] << 8U));
}

static uint32_t get_u32_le(const uint8_t *bytes)
{
    return (uint32_t)get_u16_le(bytes) |
        ((uint32_t)get_u16_le(bytes + 2) << 16U);
}

const char *nostos_result_name(nostos_result_t result)
{
    static const char *const names[] = {
        "OK", "EMPTY", "BAD_ARGUMENT", "BAD_LENGTH", "BAD_VALUE",
        "TOO_LARGE", "UNSUPPORTED_TYPE", "BAD_CRC", "TIMEOUT"
    };
    size_t index = (size_t)result;
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "UNKNOWN";
}

const char *nostos_message_type_name(uint8_t type)
{
    switch (type) {
    case NOSTOS_MESSAGE_STATE_UPDATE: return "STATE_UPDATE";
    case NOSTOS_MESSAGE_PACE_REQUEST: return "PACE_REQUEST";
    case NOSTOS_MESSAGE_STOP_REQUEST: return "STOP_REQUEST";
    case NOSTOS_MESSAGE_STOP_ACK: return "STOP_ACK";
    default: return "UNKNOWN";
    }
}

bool nostos_node_id_valid(uint8_t source_node_id)
{
    return source_node_id >= NOSTOS_NODE_ID_MIN &&
        source_node_id <= NOSTOS_NODE_ID_MAX;
}

bool nostos_request_id_valid(uint32_t request_id)
{
    return request_id != 0U;
}

static bool local_or_node_id_valid(uint8_t source_node_id)
{
    return source_node_id == NOSTOS_LOCAL_SOURCE_NODE_ID ||
        nostos_node_id_valid(source_node_id);
}

static bool pace_action_valid(uint8_t action)
{
    return action == NOSTOS_PACE_ACCELERATE ||
        action == NOSTOS_PACE_DECELERATE;
}

static bool stop_reason_valid(uint8_t reason)
{
    return reason == NOSTOS_STOP_REASON_BUTTON ||
        reason == NOSTOS_STOP_REASON_FALL;
}

static nostos_result_t encoded_size(const nostos_message_t *message,
    bool allow_local_source, size_t *size)
{
    bool source_ok = allow_local_source ?
        local_or_node_id_valid(message->source_node_id) :
        nostos_node_id_valid(message->source_node_id);
    if (!source_ok) return NOSTOS_BAD_VALUE;

    switch (message->type) {
    case NOSTOS_MESSAGE_STATE_UPDATE: {
        const nostos_state_update_t *state = &message->payload.state_update;
        if (state->schema_rev != NOSTOS_STATE_SCHEMA_REV) return NOSTOS_BAD_VALUE;
        if (state->topic_id == NOSTOS_TOPIC_RIDE) {
            *size = 11U;
            return NOSTOS_OK;
        }
        if (state->topic_id == NOSTOS_TOPIC_ENVIRONMENT) {
            *size = 9U;
            return NOSTOS_OK;
        }
        return NOSTOS_BAD_VALUE;
    }
    case NOSTOS_MESSAGE_PACE_REQUEST:
        if (!nostos_request_id_valid(message->payload.pace_request.request_id) ||
            !pace_action_valid(message->payload.pace_request.action)) {
            return NOSTOS_BAD_VALUE;
        }
        *size = 7U;
        return NOSTOS_OK;
    case NOSTOS_MESSAGE_STOP_REQUEST:
        if (!nostos_request_id_valid(message->payload.stop_request.request_id) ||
            !stop_reason_valid(message->payload.stop_request.reason)) {
            return NOSTOS_BAD_VALUE;
        }
        *size = 7U;
        return NOSTOS_OK;
    case NOSTOS_MESSAGE_STOP_ACK:
        if (!nostos_request_id_valid(message->payload.stop_ack.request_id)) {
            return NOSTOS_BAD_VALUE;
        }
        *size = 6U;
        return NOSTOS_OK;
    default:
        return NOSTOS_UNSUPPORTED_TYPE;
    }
}

static nostos_result_t message_encode_policy(const nostos_message_t *message,
    bool allow_local_source, uint8_t *wire, size_t capacity, size_t *length)
{
    if (!message || !wire || !length) return NOSTOS_BAD_ARGUMENT;
    size_t required = 0U;
    nostos_result_t result = encoded_size(message, allow_local_source, &required);
    if (result != NOSTOS_OK) return result;
    if (capacity < required) return NOSTOS_BAD_LENGTH;

    uint8_t encoded[NOSTOS_APPLICATION_MAX_SIZE] = {0};
    encoded[0] = message->type;
    encoded[1] = message->source_node_id;
    switch (message->type) {
    case NOSTOS_MESSAGE_STATE_UPDATE: {
        const nostos_state_update_t *state = &message->payload.state_update;
        encoded[2] = state->topic_id;
        encoded[3] = state->schema_rev;
        encoded[4] = state->sensor_valid ? 1U : 0U;
        if (state->sensor_valid && state->topic_id == NOSTOS_TOPIC_RIDE) {
            put_u16_le(encoded + 5, state->value.ride.speed_x10_kmh);
            put_u32_le(encoded + 7, state->value.ride.trip_distance_m);
        } else if (state->sensor_valid &&
                   state->topic_id == NOSTOS_TOPIC_ENVIRONMENT) {
            put_u16_le(encoded + 5,
                (uint16_t)state->value.environment.temperature_x10_c);
            put_u16_le(encoded + 7,
                state->value.environment.humidity_x10_pct);
        }
        break;
    }
    case NOSTOS_MESSAGE_PACE_REQUEST:
        put_u32_le(encoded + 2, message->payload.pace_request.request_id);
        encoded[6] = message->payload.pace_request.action;
        break;
    case NOSTOS_MESSAGE_STOP_REQUEST:
        put_u32_le(encoded + 2, message->payload.stop_request.request_id);
        encoded[6] = message->payload.stop_request.reason;
        break;
    case NOSTOS_MESSAGE_STOP_ACK:
        put_u32_le(encoded + 2, message->payload.stop_ack.request_id);
        break;
    default:
        return NOSTOS_UNSUPPORTED_TYPE;
    }
    memcpy(wire, encoded, required);
    *length = required;
    return NOSTOS_OK;
}

nostos_result_t nostos_message_encode(const nostos_message_t *message,
    uint8_t *wire, size_t capacity, size_t *length)
{
    return message_encode_policy(message, false, wire, capacity, length);
}

nostos_result_t nostos_local_message_encode(const nostos_message_t *message,
    uint8_t *wire, size_t capacity, size_t *length)
{
    return message_encode_policy(message, true, wire, capacity, length);
}

static nostos_result_t message_decode_policy(const uint8_t *wire, size_t length,
    bool allow_local_source, nostos_message_t *message)
{
    if (!wire || !message) return NOSTOS_BAD_ARGUMENT;
    if (length > NOSTOS_APPLICATION_MAX_SIZE) return NOSTOS_TOO_LARGE;
    if (length < NOSTOS_APPLICATION_MIN_SIZE) return NOSTOS_BAD_LENGTH;
    bool source_ok = allow_local_source ? local_or_node_id_valid(wire[1]) :
        nostos_node_id_valid(wire[1]);
    if (!source_ok) return NOSTOS_BAD_VALUE;

    nostos_message_t decoded = {0};
    decoded.type = wire[0];
    decoded.source_node_id = wire[1];
    switch (wire[0]) {
    case NOSTOS_MESSAGE_STATE_UPDATE: {
        uint8_t topic = wire[2];
        size_t expected = topic == NOSTOS_TOPIC_RIDE ? 11U :
            topic == NOSTOS_TOPIC_ENVIRONMENT ? 9U : 0U;
        if (expected == 0U) return NOSTOS_BAD_VALUE;
        if (length != expected) return NOSTOS_BAD_LENGTH;
        if (wire[3] != NOSTOS_STATE_SCHEMA_REV || wire[4] > 1U) {
            return NOSTOS_BAD_VALUE;
        }
        decoded.payload.state_update.topic_id = topic;
        decoded.payload.state_update.schema_rev = wire[3];
        decoded.payload.state_update.sensor_valid = wire[4] != 0U;
        if (topic == NOSTOS_TOPIC_RIDE) {
            uint16_t speed = get_u16_le(wire + 5);
            uint32_t distance = get_u32_le(wire + 7);
            if (wire[4] == 0U && (speed != 0U || distance != 0U)) {
                return NOSTOS_BAD_VALUE;
            }
            decoded.payload.state_update.value.ride =
                (nostos_ride_state_t){speed, distance};
        } else {
            int16_t temperature = (int16_t)get_u16_le(wire + 5);
            uint16_t humidity = get_u16_le(wire + 7);
            if (wire[4] == 0U && (temperature != 0 || humidity != 0U)) {
                return NOSTOS_BAD_VALUE;
            }
            decoded.payload.state_update.value.environment =
                (nostos_environment_state_t){temperature, humidity};
        }
        break;
    }
    case NOSTOS_MESSAGE_PACE_REQUEST:
        if (length != 7U) return NOSTOS_BAD_LENGTH;
        decoded.payload.pace_request.request_id = get_u32_le(wire + 2);
        decoded.payload.pace_request.action = wire[6];
        if (!nostos_request_id_valid(decoded.payload.pace_request.request_id) ||
            !pace_action_valid(decoded.payload.pace_request.action)) {
            return NOSTOS_BAD_VALUE;
        }
        break;
    case NOSTOS_MESSAGE_STOP_REQUEST:
        if (length != 7U) return NOSTOS_BAD_LENGTH;
        decoded.payload.stop_request.request_id = get_u32_le(wire + 2);
        decoded.payload.stop_request.reason = wire[6];
        if (!nostos_request_id_valid(decoded.payload.stop_request.request_id) ||
            !stop_reason_valid(decoded.payload.stop_request.reason)) {
            return NOSTOS_BAD_VALUE;
        }
        break;
    case NOSTOS_MESSAGE_STOP_ACK:
        if (length != 6U) return NOSTOS_BAD_LENGTH;
        decoded.payload.stop_ack.request_id = get_u32_le(wire + 2);
        if (!nostos_request_id_valid(decoded.payload.stop_ack.request_id)) {
            return NOSTOS_BAD_VALUE;
        }
        break;
    default:
        return NOSTOS_UNSUPPORTED_TYPE;
    }
    *message = decoded;
    return NOSTOS_OK;
}

nostos_result_t nostos_message_decode(const uint8_t *wire, size_t length,
    nostos_message_t *message)
{
    return message_decode_policy(wire, length, false, message);
}

nostos_result_t nostos_local_message_decode(const uint8_t *wire, size_t length,
    nostos_message_t *message)
{
    return message_decode_policy(wire, length, true, message);
}

static bool constructor_source_valid(uint8_t source_node_id)
{
    return local_or_node_id_valid(source_node_id);
}

nostos_result_t nostos_message_make_ride(nostos_message_t *message,
    uint8_t source_node_id, bool sensor_valid, uint16_t speed_x10_kmh,
    uint32_t trip_distance_m)
{
    if (!message) return NOSTOS_BAD_ARGUMENT;
    if (!constructor_source_valid(source_node_id)) return NOSTOS_BAD_VALUE;
    *message = (nostos_message_t){
        .type = NOSTOS_MESSAGE_STATE_UPDATE,
        .source_node_id = source_node_id,
        .payload.state_update = {
            .topic_id = NOSTOS_TOPIC_RIDE,
            .schema_rev = NOSTOS_STATE_SCHEMA_REV,
            .sensor_valid = sensor_valid,
            .value.ride = sensor_valid ?
                (nostos_ride_state_t){speed_x10_kmh, trip_distance_m} :
                (nostos_ride_state_t){0U, 0U}
        }
    };
    return NOSTOS_OK;
}

nostos_result_t nostos_message_make_environment(nostos_message_t *message,
    uint8_t source_node_id, bool sensor_valid, int16_t temperature_x10_c,
    uint16_t humidity_x10_pct)
{
    if (!message) return NOSTOS_BAD_ARGUMENT;
    if (!constructor_source_valid(source_node_id)) return NOSTOS_BAD_VALUE;
    *message = (nostos_message_t){
        .type = NOSTOS_MESSAGE_STATE_UPDATE,
        .source_node_id = source_node_id,
        .payload.state_update = {
            .topic_id = NOSTOS_TOPIC_ENVIRONMENT,
            .schema_rev = NOSTOS_STATE_SCHEMA_REV,
            .sensor_valid = sensor_valid,
            .value.environment = sensor_valid ?
                (nostos_environment_state_t){temperature_x10_c,
                    humidity_x10_pct} :
                (nostos_environment_state_t){0, 0U}
        }
    };
    return NOSTOS_OK;
}

nostos_result_t nostos_message_make_pace(nostos_message_t *message,
    uint8_t source_node_id, uint32_t request_id, uint8_t action)
{
    if (!message) return NOSTOS_BAD_ARGUMENT;
    if (!constructor_source_valid(source_node_id) ||
        !nostos_request_id_valid(request_id) || !pace_action_valid(action)) {
        return NOSTOS_BAD_VALUE;
    }
    *message = (nostos_message_t){
        .type = NOSTOS_MESSAGE_PACE_REQUEST,
        .source_node_id = source_node_id,
        .payload.pace_request = {request_id, action}
    };
    return NOSTOS_OK;
}

nostos_result_t nostos_message_make_stop(nostos_message_t *message,
    uint8_t source_node_id, uint32_t request_id, uint8_t reason)
{
    if (!message) return NOSTOS_BAD_ARGUMENT;
    if (!constructor_source_valid(source_node_id) ||
        !nostos_request_id_valid(request_id) || !stop_reason_valid(reason)) {
        return NOSTOS_BAD_VALUE;
    }
    *message = (nostos_message_t){
        .type = NOSTOS_MESSAGE_STOP_REQUEST,
        .source_node_id = source_node_id,
        .payload.stop_request = {request_id, reason}
    };
    return NOSTOS_OK;
}

nostos_result_t nostos_message_make_stop_ack(nostos_message_t *message,
    uint8_t source_node_id, uint32_t request_id)
{
    if (!message) return NOSTOS_BAD_ARGUMENT;
    if (!constructor_source_valid(source_node_id) ||
        !nostos_request_id_valid(request_id)) return NOSTOS_BAD_VALUE;
    *message = (nostos_message_t){
        .type = NOSTOS_MESSAGE_STOP_ACK,
        .source_node_id = source_node_id,
        .payload.stop_ack = {request_id}
    };
    return NOSTOS_OK;
}
