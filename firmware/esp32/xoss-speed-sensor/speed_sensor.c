#include "speed_sensor.h"

#include <limits.h>
#include <string.h>

enum {
    CSC_FLAG_WHEEL = 1U << 0,
    CSC_FLAG_CRANK = 1U << 1,
    CSC_FLAG_RESERVED_MASK = 0xFCU
};

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      (uint16_t)((uint16_t)bytes[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)read_u16_le(bytes) |
           ((uint32_t)read_u16_le(bytes + 2) << 16);
}

speed_sensor_result_t speed_sensor_decode_csc(
    const uint8_t *payload,
    size_t length,
    speed_sensor_measurement_t *measurement)
{
    if (payload == NULL || measurement == NULL) {
        return SPEED_SENSOR_BAD_ARGUMENT;
    }
    if (length < 1U) {
        return SPEED_SENSOR_BAD_LENGTH;
    }

    const uint8_t flags = payload[0];
    if ((flags & CSC_FLAG_RESERVED_MASK) != 0U) {
        return SPEED_SENSOR_BAD_FLAGS;
    }

    const bool has_wheel = (flags & CSC_FLAG_WHEEL) != 0U;
    const bool has_crank = (flags & CSC_FLAG_CRANK) != 0U;
    const size_t expected_length = 1U + (has_wheel ? 6U : 0U) +
                                   (has_crank ? 4U : 0U);
    if (length != expected_length) {
        return SPEED_SENSOR_BAD_LENGTH;
    }

    speed_sensor_measurement_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    decoded.has_wheel = has_wheel;
    decoded.has_crank = has_crank;

    size_t offset = 1U;
    if (has_wheel) {
        decoded.cumulative_wheel_revolutions = read_u32_le(payload + offset);
        decoded.last_wheel_event_time_1024 = read_u16_le(payload + offset + 4U);
        offset += 6U;
    }
    if (has_crank) {
        decoded.cumulative_crank_revolutions = read_u16_le(payload + offset);
        decoded.last_crank_event_time_1024 = read_u16_le(payload + offset + 2U);
    }

    *measurement = decoded;
    return SPEED_SENSOR_OK;
}

speed_sensor_result_t speed_sensor_update(
    speed_sensor_state_t *state,
    const speed_sensor_measurement_t *measurement,
    uint32_t travel_circumference_mm,
    speed_sensor_sample_t *sample)
{
    if (state == NULL || measurement == NULL || sample == NULL ||
        travel_circumference_mm == 0U) {
        return SPEED_SENSOR_BAD_ARGUMENT;
    }

    const speed_sensor_sample_t invalid_sample = {
        .valid = false,
        .kmh_x10 = 0U,
        .delta_revolutions = 0U,
        .distance_mm = state->distance_mm
    };
    *sample = invalid_sample;

    if (!measurement->has_wheel) {
        return SPEED_SENSOR_NO_WHEEL_DATA;
    }

    if (!state->has_baseline) {
        state->has_baseline = true;
        state->previous_wheel_revolutions =
            measurement->cumulative_wheel_revolutions;
        state->previous_wheel_event_time_1024 =
            measurement->last_wheel_event_time_1024;
        return SPEED_SENSOR_BASELINE;
    }

    if (measurement->cumulative_wheel_revolutions <
        state->previous_wheel_revolutions) {
        state->previous_wheel_revolutions =
            measurement->cumulative_wheel_revolutions;
        state->previous_wheel_event_time_1024 =
            measurement->last_wheel_event_time_1024;
        return SPEED_SENSOR_REBASELINE;
    }

    const uint32_t delta_revolutions =
        measurement->cumulative_wheel_revolutions -
        state->previous_wheel_revolutions;
    const uint16_t delta_ticks = (uint16_t)(
        measurement->last_wheel_event_time_1024 -
        state->previous_wheel_event_time_1024);

    if (delta_ticks == 0U) {
        return SPEED_SENSOR_NO_UPDATE;
    }

    if (delta_revolutions == 0U) {
        state->previous_wheel_revolutions =
            measurement->cumulative_wheel_revolutions;
        state->previous_wheel_event_time_1024 =
            measurement->last_wheel_event_time_1024;
        return SPEED_SENSOR_NO_UPDATE;
    }

    if ((uint64_t)delta_revolutions >
        UINT64_MAX / (uint64_t)travel_circumference_mm) {
        return SPEED_SENSOR_OUT_OF_RANGE;
    }
    const uint64_t delta_distance_mm =
        (uint64_t)delta_revolutions * travel_circumference_mm;
    if (state->distance_mm > UINT64_MAX - delta_distance_mm) {
        return SPEED_SENSOR_OUT_OF_RANGE;
    }

    /* km/h x10 = revolutions * circumference_mm * 1024 * 36 /
     *              (event_ticks * 1000). Round to nearest integer. */
    const uint64_t scale = (uint64_t)travel_circumference_mm * 1024U * 36U;
    if ((uint64_t)delta_revolutions > UINT64_MAX / scale) {
        return SPEED_SENSOR_OUT_OF_RANGE;
    }

    const uint64_t numerator = (uint64_t)delta_revolutions * scale;
    const uint64_t denominator = (uint64_t)delta_ticks * 1000U;
    const uint64_t rounded_kmh_x10 =
        (numerator + (denominator / 2U)) / denominator;
    if (rounded_kmh_x10 > UINT16_MAX) {
        return SPEED_SENSOR_OUT_OF_RANGE;
    }

    state->previous_wheel_revolutions =
        measurement->cumulative_wheel_revolutions;
    state->previous_wheel_event_time_1024 =
        measurement->last_wheel_event_time_1024;
    state->distance_mm += delta_distance_mm;

    sample->valid = true;
    sample->kmh_x10 = (uint16_t)rounded_kmh_x10;
    sample->delta_revolutions = delta_revolutions;
    sample->distance_mm = state->distance_mm;
    return SPEED_SENSOR_OK;
}

void speed_sensor_reset(speed_sensor_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

const char *speed_sensor_result_name(speed_sensor_result_t result)
{
    static const char *const names[] = {
        "OK",
        "BASELINE",
        "REBASELINE",
        "NO_UPDATE",
        "NO_WHEEL_DATA",
        "BAD_ARGUMENT",
        "BAD_FLAGS",
        "BAD_LENGTH",
        "OUT_OF_RANGE"
    };
    const size_t count = sizeof(names) / sizeof(names[0]);
    return (size_t)result < count ? names[result] : "UNKNOWN";
}
