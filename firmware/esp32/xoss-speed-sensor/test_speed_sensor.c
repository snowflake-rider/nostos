#include "speed_sensor.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                       \
    do {                                                                        \
        if (!(expression)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
                    __FILE__, __LINE__, #expression);                           \
            return false;                                                       \
        }                                                                       \
    } while (0)

enum { ARM_DEMO_CIRCUMFERENCE_MM = 5100U };

static speed_sensor_measurement_t wheel_measurement(uint32_t revolutions,
                                                     uint16_t event_time)
{
    speed_sensor_measurement_t measurement;
    memset(&measurement, 0, sizeof(measurement));
    measurement.has_wheel = true;
    measurement.cumulative_wheel_revolutions = revolutions;
    measurement.last_wheel_event_time_1024 = event_time;
    return measurement;
}

static bool test_captured_xoss_sequence(void)
{
    static const uint8_t notifications[][7] = {
        {0x01U, 0x38U, 0x00U, 0x00U, 0x00U, 0x6AU, 0x36U},
        {0x01U, 0x3DU, 0x00U, 0x00U, 0x00U, 0xA2U, 0x46U},
        {0x01U, 0x42U, 0x00U, 0x00U, 0x00U, 0xBBU, 0x56U},
        {0x01U, 0x48U, 0x00U, 0x00U, 0x00U, 0x17U, 0x6AU}
    };
    static const uint32_t expected_revolutions[] = {56U, 61U, 66U, 72U};
    static const uint16_t expected_event_times[] = {
        13930U, 18082U, 22203U, 27159U
    };
    static const uint16_t expected_kmh_x10[] = {0U, 226U, 228U, 228U};
    static const uint32_t expected_delta_revolutions[] = {0U, 5U, 5U, 6U};
    static const uint64_t expected_distance_mm[] = {
        0U, 25500U, 51000U, 81600U
    };

    speed_sensor_state_t state = {0};
    for (size_t i = 0U; i < 4U; ++i) {
        speed_sensor_measurement_t measurement;
        speed_sensor_sample_t sample = {
            .valid = true,
            .kmh_x10 = UINT16_MAX,
            .delta_revolutions = UINT32_MAX,
            .distance_mm = UINT64_MAX
        };
        CHECK(speed_sensor_decode_csc(notifications[i], sizeof(notifications[i]),
                                      &measurement) == SPEED_SENSOR_OK);
        CHECK(measurement.has_wheel && !measurement.has_crank);
        CHECK(measurement.cumulative_wheel_revolutions ==
              expected_revolutions[i]);
        CHECK(measurement.last_wheel_event_time_1024 == expected_event_times[i]);

        const speed_sensor_result_t result = speed_sensor_update(
            &state, &measurement, ARM_DEMO_CIRCUMFERENCE_MM, &sample);
        if (i == 0U) {
            CHECK(result == SPEED_SENSOR_BASELINE);
            CHECK(!sample.valid && sample.kmh_x10 == 0U);
        } else {
            CHECK(result == SPEED_SENSOR_OK);
            CHECK(sample.valid && sample.kmh_x10 == expected_kmh_x10[i]);
        }
        CHECK(sample.delta_revolutions == expected_delta_revolutions[i]);
        CHECK(sample.distance_mm == expected_distance_mm[i]);
        CHECK(state.distance_mm == expected_distance_mm[i]);
    }
    return true;
}

static bool test_decode_wheel_and_crank(void)
{
    const uint8_t payload[] = {
        0x03U,
        0x78U, 0x56U, 0x34U, 0x12U, 0xCDU, 0xABU,
        0x34U, 0x12U, 0x78U, 0x56U
    };
    speed_sensor_measurement_t measurement;
    CHECK(speed_sensor_decode_csc(payload, sizeof(payload), &measurement) ==
          SPEED_SENSOR_OK);
    CHECK(measurement.has_wheel && measurement.has_crank);
    CHECK(measurement.cumulative_wheel_revolutions == 0x12345678U);
    CHECK(measurement.last_wheel_event_time_1024 == 0xABCDU);
    CHECK(measurement.cumulative_crank_revolutions == 0x1234U);
    CHECK(measurement.last_crank_event_time_1024 == 0x5678U);
    return true;
}

static bool test_rejects_bad_flags_and_lengths(void)
{
    const uint8_t bad_flags[] = {0x04U};
    const uint8_t short_wheel[] = {0x01U, 0x00U};
    speed_sensor_measurement_t measurement = {0};
    CHECK(speed_sensor_decode_csc(bad_flags, sizeof(bad_flags), &measurement) ==
          SPEED_SENSOR_BAD_FLAGS);
    CHECK(speed_sensor_decode_csc(short_wheel, sizeof(short_wheel),
                                  &measurement) == SPEED_SENSOR_BAD_LENGTH);
    CHECK(speed_sensor_decode_csc(short_wheel, 0U, &measurement) ==
          SPEED_SENSOR_BAD_LENGTH);
    return true;
}

static bool test_cadence_only_has_no_speed(void)
{
    const uint8_t payload[] = {0x02U, 0x01U, 0x00U, 0x00U, 0x04U};
    speed_sensor_measurement_t measurement;
    speed_sensor_state_t state = {0};
    speed_sensor_sample_t sample = {
        .valid = true,
        .kmh_x10 = 999U,
        .delta_revolutions = 999U,
        .distance_mm = 999U
    };
    CHECK(speed_sensor_decode_csc(payload, sizeof(payload), &measurement) ==
          SPEED_SENSOR_OK);
    CHECK(speed_sensor_update(&state, &measurement, 2105U, &sample) ==
          SPEED_SENSOR_NO_WHEEL_DATA);
    CHECK(!sample.valid && sample.kmh_x10 == 0U);
    CHECK(sample.delta_revolutions == 0U && sample.distance_mm == 0U);
    CHECK(!state.has_baseline);
    return true;
}

static bool test_event_time_wraparound(void)
{
    speed_sensor_state_t state = {0};
    speed_sensor_sample_t sample;
    const speed_sensor_measurement_t first = wheel_measurement(100U, 0xFF00U);
    const speed_sensor_measurement_t second = wheel_measurement(101U, 0x0300U);
    CHECK(speed_sensor_update(&state, &first, 2105U, &sample) ==
          SPEED_SENSOR_BASELINE);
    CHECK(speed_sensor_update(&state, &second, 2105U, &sample) ==
          SPEED_SENSOR_OK);
    CHECK(sample.valid && sample.kmh_x10 == 76U);
    CHECK(sample.delta_revolutions == 1U && sample.distance_mm == 2105U);
    return true;
}

static bool test_zero_tick_delta_does_not_advance_baseline(void)
{
    speed_sensor_state_t state = {0};
    speed_sensor_sample_t sample;
    const speed_sensor_measurement_t first = wheel_measurement(10U, 100U);
    const speed_sensor_measurement_t impossible = wheel_measurement(11U, 100U);
    const speed_sensor_measurement_t recovered = wheel_measurement(11U, 1124U);
    CHECK(speed_sensor_update(&state, &first, 2105U, &sample) ==
          SPEED_SENSOR_BASELINE);
    CHECK(speed_sensor_update(&state, &impossible, 2105U, &sample) ==
          SPEED_SENSOR_NO_UPDATE);
    CHECK(state.previous_wheel_revolutions == 10U);
    CHECK(state.distance_mm == 0U && sample.distance_mm == 0U);
    CHECK(speed_sensor_update(&state, &recovered, 2105U, &sample) ==
          SPEED_SENSOR_OK);
    CHECK(sample.valid && sample.kmh_x10 == 76U);
    CHECK(sample.distance_mm == 2105U);
    return true;
}

static bool test_decreasing_revolutions_rebaseline(void)
{
    speed_sensor_state_t state = {0};
    speed_sensor_sample_t sample;
    const speed_sensor_measurement_t first = wheel_measurement(100U, 1000U);
    const speed_sensor_measurement_t forward = wheel_measurement(101U, 2024U);
    const speed_sensor_measurement_t reversed = wheel_measurement(99U, 2200U);
    CHECK(speed_sensor_update(&state, &first, 2105U, &sample) ==
          SPEED_SENSOR_BASELINE);
    CHECK(speed_sensor_update(&state, &forward, 2105U, &sample) ==
          SPEED_SENSOR_OK);
    CHECK(sample.distance_mm == 2105U);
    CHECK(speed_sensor_update(&state, &reversed, 2105U, &sample) ==
          SPEED_SENSOR_REBASELINE);
    CHECK(!sample.valid && state.previous_wheel_revolutions == 99U);
    CHECK(sample.distance_mm == 2105U && state.distance_mm == 2105U);
    return true;
}

static bool test_reset_and_bad_arguments(void)
{
    speed_sensor_state_t state = {
        .has_baseline = true,
        .previous_wheel_revolutions = 10U,
        .previous_wheel_event_time_1024 = 20U,
        .distance_mm = 1234U
    };
    speed_sensor_measurement_t measurement = wheel_measurement(11U, 1044U);
    speed_sensor_sample_t sample;
    speed_sensor_reset(&state);
    CHECK(!state.has_baseline && state.distance_mm == 0U);
    CHECK(speed_sensor_update(&state, &measurement, 0U, &sample) ==
          SPEED_SENSOR_BAD_ARGUMENT);
    CHECK(speed_sensor_update(NULL, &measurement, 2105U, &sample) ==
          SPEED_SENSOR_BAD_ARGUMENT);
    CHECK(speed_sensor_decode_csc(NULL, 0U, &measurement) ==
          SPEED_SENSOR_BAD_ARGUMENT);
    return true;
}

static bool test_out_of_range_is_invalid(void)
{
    speed_sensor_state_t state = {0};
    speed_sensor_sample_t sample;
    const speed_sensor_measurement_t first = wheel_measurement(0U, 0U);
    const speed_sensor_measurement_t second =
        wheel_measurement(UINT32_MAX, 1U);
    CHECK(speed_sensor_update(&state, &first, UINT32_MAX, &sample) ==
          SPEED_SENSOR_BASELINE);
    CHECK(speed_sensor_update(&state, &second, UINT32_MAX, &sample) ==
          SPEED_SENSOR_OUT_OF_RANGE);
    CHECK(!sample.valid && sample.kmh_x10 == 0U);

    state = (speed_sensor_state_t){
        .has_baseline = true,
        .previous_wheel_revolutions = 0U,
        .previous_wheel_event_time_1024 = 0U,
        .distance_mm = UINT64_MAX - 1000U
    };
    const speed_sensor_measurement_t distance_overflow =
        wheel_measurement(1U, 1024U);
    CHECK(speed_sensor_update(&state, &distance_overflow, 2105U, &sample) ==
          SPEED_SENSOR_OUT_OF_RANGE);
    CHECK(!sample.valid && sample.distance_mm == UINT64_MAX - 1000U);
    CHECK(state.distance_mm == UINT64_MAX - 1000U);
    return true;
}

static bool test_uint32_transport_boundary_remains_visible(void)
{
    speed_sensor_state_t state = {
        .has_baseline = true,
        .previous_wheel_revolutions = 10U,
        .previous_wheel_event_time_1024 = 0U,
        .distance_mm = (uint64_t)UINT32_MAX - 1000U
    };
    const speed_sensor_measurement_t next = wheel_measurement(11U, 1024U);
    speed_sensor_sample_t sample;
    CHECK(speed_sensor_update(&state, &next, 2105U, &sample) == SPEED_SENSOR_OK);
    CHECK(sample.valid && sample.kmh_x10 == 76U);
    CHECK(sample.distance_mm == (uint64_t)UINT32_MAX + 1105U);
    CHECK(sample.distance_mm > UINT32_MAX);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"captured XOSS speed and distance", test_captured_xoss_sequence},
        {"decode wheel and crank", test_decode_wheel_and_crank},
        {"reject flags and lengths", test_rejects_bad_flags_and_lengths},
        {"cadence only", test_cadence_only_has_no_speed},
        {"event time wrap", test_event_time_wraparound},
        {"zero tick delta", test_zero_tick_delta_does_not_advance_baseline},
        {"decreasing revolutions", test_decreasing_revolutions_rebaseline},
        {"reset and arguments", test_reset_and_bad_arguments},
        {"out of range", test_out_of_range_is_invalid},
        {"uint32 transport boundary", test_uint32_transport_boundary_remains_visible}
    };

    for (size_t i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!tests[i].run()) {
            fprintf(stderr, "FAIL: %s\n", tests[i].name);
            return 1;
        }
        printf("PASS: %s\n", tests[i].name);
    }
    return 0;
}
