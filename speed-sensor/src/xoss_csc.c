#include "xoss_csc.h"

#include <limits.h>
#include <string.h>

enum {
    CSC_FLAG_WHEEL = 1U << 0,
    CSC_FLAG_CRANK = 1U << 1
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

xoss_csc_result_t xoss_csc_decode(const uint8_t *payload,
                                  size_t length,
                                  xoss_csc_measurement_t *measurement)
{
    if (payload == NULL || measurement == NULL) {
        return XOSS_CSC_BAD_ARGUMENT;
    }
    if (length < 1U) {
        return XOSS_CSC_BAD_LENGTH;
    }

    const uint8_t flags = payload[0];
    const bool has_wheel = (flags & CSC_FLAG_WHEEL) != 0U;
    const bool has_crank = (flags & CSC_FLAG_CRANK) != 0U;
    const size_t expected_length = 1U + (has_wheel ? 6U : 0U) +
                                   (has_crank ? 4U : 0U);
    if (length != expected_length) {
        return XOSS_CSC_BAD_LENGTH;
    }

    xoss_csc_measurement_t decoded;
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
    return XOSS_CSC_OK;
}

xoss_csc_result_t xoss_csc_speed_update(xoss_csc_speed_state_t *state,
                                        const xoss_csc_measurement_t *measurement,
                                        uint16_t wheel_circumference_mm,
                                        xoss_speed_sample_t *sample)
{
    if (state == NULL || measurement == NULL || sample == NULL ||
        wheel_circumference_mm == 0U) {
        return XOSS_CSC_BAD_ARGUMENT;
    }

    const xoss_speed_sample_t invalid_sample = {false, 0U};
    if (!measurement->has_wheel) {
        *sample = invalid_sample;
        return XOSS_CSC_NO_WHEEL_DATA;
    }

    if (!state->has_baseline) {
        state->has_baseline = true;
        state->previous_wheel_revolutions =
            measurement->cumulative_wheel_revolutions;
        state->previous_wheel_event_time_1024 =
            measurement->last_wheel_event_time_1024;
        *sample = invalid_sample;
        return XOSS_CSC_BASELINE;
    }

    const uint32_t delta_revolutions =
        measurement->cumulative_wheel_revolutions -
        state->previous_wheel_revolutions;
    const uint16_t delta_ticks = (uint16_t)(
        measurement->last_wheel_event_time_1024 -
        state->previous_wheel_event_time_1024);

    if (delta_ticks == 0U) {
        *sample = invalid_sample;
        return XOSS_CSC_NO_UPDATE;
    }

    state->previous_wheel_revolutions =
        measurement->cumulative_wheel_revolutions;
    state->previous_wheel_event_time_1024 =
        measurement->last_wheel_event_time_1024;

    /* km/h x10 = revolutions * circumference_mm * 1024 * 36 /
     *              (1000 * event_ticks). Round to nearest integer. */
    const uint64_t numerator = (uint64_t)delta_revolutions *
                               wheel_circumference_mm * 1024U * 36U;
    const uint64_t denominator = (uint64_t)delta_ticks * 1000U;
    const uint64_t rounded_kmh_x10 =
        (numerator + (denominator / 2U)) / denominator;

    if (rounded_kmh_x10 > UINT16_MAX) {
        *sample = invalid_sample;
        return XOSS_CSC_OUT_OF_RANGE;
    }

    sample->valid = true;
    sample->kmh_x10 = (uint16_t)rounded_kmh_x10;
    return XOSS_CSC_OK;
}

void xoss_csc_speed_reset(xoss_csc_speed_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

const char *xoss_csc_result_name(xoss_csc_result_t result)
{
    static const char *const names[] = {
        "OK",
        "BASELINE",
        "NO_UPDATE",
        "NO_WHEEL_DATA",
        "BAD_ARGUMENT",
        "BAD_LENGTH",
        "OUT_OF_RANGE"
    };
    const size_t count = sizeof(names) / sizeof(names[0]);
    return (size_t)result < count ? names[result] : "UNKNOWN";
}
