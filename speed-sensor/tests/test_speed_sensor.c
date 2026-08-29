#include "speed_sensor_local.h"
#include "nostos_uart.h"
#include "xoss_csc.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                     \
                    __FILE__, __LINE__, #expression);                            \
            return false;                                                        \
        }                                                                        \
    } while (0)

static xoss_csc_measurement_t wheel_measurement(uint32_t revolutions,
                                                 uint16_t event_time)
{
    xoss_csc_measurement_t measurement;
    memset(&measurement, 0, sizeof(measurement));
    measurement.has_wheel = true;
    measurement.cumulative_wheel_revolutions = revolutions;
    measurement.last_wheel_event_time_1024 = event_time;
    return measurement;
}

static bool test_decode_wheel_only(void)
{
    const uint8_t payload[] = {0x01U, 0x10U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x04U};
    xoss_csc_measurement_t measurement;
    CHECK(xoss_csc_decode(payload, sizeof(payload), &measurement) == XOSS_CSC_OK);
    CHECK(measurement.has_wheel);
    CHECK(!measurement.has_crank);
    CHECK(measurement.cumulative_wheel_revolutions == 16U);
    CHECK(measurement.last_wheel_event_time_1024 == 1024U);
    return true;
}

static bool test_decode_wheel_and_crank(void)
{
    const uint8_t payload[] = {
        0x03U,
        0x78U, 0x56U, 0x34U, 0x12U, 0xCDU, 0xABU,
        0x34U, 0x12U, 0x78U, 0x56U
    };
    xoss_csc_measurement_t measurement;
    CHECK(xoss_csc_decode(payload, sizeof(payload), &measurement) == XOSS_CSC_OK);
    CHECK(measurement.has_wheel && measurement.has_crank);
    CHECK(measurement.cumulative_wheel_revolutions == 0x12345678U);
    CHECK(measurement.last_wheel_event_time_1024 == 0xABCDU);
    CHECK(measurement.cumulative_crank_revolutions == 0x1234U);
    CHECK(measurement.last_crank_event_time_1024 == 0x5678U);
    return true;
}

static bool test_decode_rejects_bad_length(void)
{
    const uint8_t payload[] = {0x01U, 0x00U};
    xoss_csc_measurement_t measurement = {0};
    CHECK(xoss_csc_decode(payload, sizeof(payload), &measurement) ==
          XOSS_CSC_BAD_LENGTH);
    return true;
}

static bool test_cadence_only_has_no_speed(void)
{
    const uint8_t payload[] = {0x02U, 0x01U, 0x00U, 0x00U, 0x04U};
    xoss_csc_measurement_t measurement;
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample = {true, 999U};
    CHECK(xoss_csc_decode(payload, sizeof(payload), &measurement) == XOSS_CSC_OK);
    CHECK(xoss_csc_speed_update(&state, &measurement, 2105U, &sample) ==
          XOSS_CSC_NO_WHEEL_DATA);
    CHECK(!sample.valid && sample.kmh_x10 == 0U);
    CHECK(!state.has_baseline);
    return true;
}

static bool test_speed_example_rounds_to_76(void)
{
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample;
    xoss_csc_measurement_t first = wheel_measurement(16U, 0x0400U);
    xoss_csc_measurement_t second = wheel_measurement(17U, 0x0800U);
    CHECK(xoss_csc_speed_update(&state, &first, 2105U, &sample) ==
          XOSS_CSC_BASELINE);
    CHECK(xoss_csc_speed_update(&state, &second, 2105U, &sample) == XOSS_CSC_OK);
    CHECK(sample.valid && sample.kmh_x10 == 76U);
    return true;
}

static bool test_event_time_wraparound(void)
{
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample;
    xoss_csc_measurement_t first = wheel_measurement(100U, 0xFF00U);
    xoss_csc_measurement_t second = wheel_measurement(101U, 0x0300U);
    CHECK(xoss_csc_speed_update(&state, &first, 2105U, &sample) ==
          XOSS_CSC_BASELINE);
    CHECK(xoss_csc_speed_update(&state, &second, 2105U, &sample) == XOSS_CSC_OK);
    CHECK(sample.kmh_x10 == 76U);
    return true;
}

static bool test_revolution_wraparound(void)
{
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample;
    xoss_csc_measurement_t first = wheel_measurement(UINT32_MAX, 0x0400U);
    xoss_csc_measurement_t second = wheel_measurement(0U, 0x0800U);
    CHECK(xoss_csc_speed_update(&state, &first, 2105U, &sample) ==
          XOSS_CSC_BASELINE);
    CHECK(xoss_csc_speed_update(&state, &second, 2105U, &sample) == XOSS_CSC_OK);
    CHECK(sample.kmh_x10 == 76U);
    return true;
}

static bool test_zero_delta_does_not_advance_baseline(void)
{
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample;
    xoss_csc_measurement_t first = wheel_measurement(10U, 100U);
    xoss_csc_measurement_t impossible = wheel_measurement(11U, 100U);
    xoss_csc_measurement_t recovered = wheel_measurement(11U, 1124U);
    CHECK(xoss_csc_speed_update(&state, &first, 2105U, &sample) ==
          XOSS_CSC_BASELINE);
    CHECK(xoss_csc_speed_update(&state, &impossible, 2105U, &sample) ==
          XOSS_CSC_NO_UPDATE);
    CHECK(state.previous_wheel_revolutions == 10U);
    CHECK(xoss_csc_speed_update(&state, &recovered, 2105U, &sample) == XOSS_CSC_OK);
    CHECK(sample.kmh_x10 == 76U);
    return true;
}

static bool test_reset_requires_new_baseline(void)
{
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample;
    xoss_csc_measurement_t measurement = wheel_measurement(20U, 1000U);
    CHECK(xoss_csc_speed_update(&state, &measurement, 2105U, &sample) ==
          XOSS_CSC_BASELINE);
    xoss_csc_speed_reset(&state);
    CHECK(!state.has_baseline);
    CHECK(xoss_csc_speed_update(&state, &measurement, 2105U, &sample) ==
          XOSS_CSC_BASELINE);
    return true;
}

static bool test_out_of_range_is_invalid(void)
{
    xoss_csc_speed_state_t state = {0};
    xoss_speed_sample_t sample;
    xoss_csc_measurement_t first = wheel_measurement(0U, 0U);
    xoss_csc_measurement_t second = wheel_measurement(UINT32_MAX, 1U);
    CHECK(xoss_csc_speed_update(&state, &first, UINT16_MAX, &sample) ==
          XOSS_CSC_BASELINE);
    CHECK(xoss_csc_speed_update(&state, &second, UINT16_MAX, &sample) ==
          XOSS_CSC_OUT_OF_RANGE);
    CHECK(!sample.valid && sample.kmh_x10 == 0U);
    return true;
}

static bool test_local_payload_roundtrip(void)
{
    const xoss_speed_sample_t input = {true, 76U};
    const uint8_t expected[SPEED_SENSOR_LOCAL_PAYLOAD_SIZE] = {
        0xA5U, 0x5AU, 0x01U, 0x01U, 0x01U, 0x4CU, 0x00U, 0x00U, 0x00U
    };
    uint8_t payload[SPEED_SENSOR_LOCAL_PAYLOAD_SIZE];
    xoss_speed_sample_t output = {0};
    CHECK(speed_sensor_local_encode(&input, payload, sizeof(payload)));
    CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    CHECK(speed_sensor_local_decode(payload, sizeof(payload), &output));
    CHECK(output.valid && output.kmh_x10 == 76U);
    return true;
}

static bool test_invalid_local_payload_is_rejected(void)
{
    const xoss_speed_sample_t invalid_input = {false, 1U};
    uint8_t payload[SPEED_SENSOR_LOCAL_PAYLOAD_SIZE] = {0};
    CHECK(!speed_sensor_local_encode(&invalid_input, payload, sizeof(payload)));

    const uint8_t malformed[SPEED_SENSOR_LOCAL_PAYLOAD_SIZE] = {
        0xA5U, 0x5AU, 0x01U, 0x01U, 0x00U, 0x01U, 0x00U, 0x00U, 0x00U
    };
    xoss_speed_sample_t output = {0};
    CHECK(!speed_sensor_local_decode(malformed, sizeof(malformed), &output));
    return true;
}

static bool test_local_payload_uses_existing_uart_framer(void)
{
    const xoss_speed_sample_t sample = {true, 76U};
    uint8_t payload[SPEED_SENSOR_LOCAL_PAYLOAD_SIZE];
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    uint8_t decoded[NOSTOS_WIRE_MAX];
    size_t frame_length = 0U;
    size_t decoded_length = 0U;
    nostos_uart_parser_t parser = {0};

    CHECK(speed_sensor_local_encode(&sample, payload, sizeof(payload)));
    CHECK(nostos_uart_encode(payload, sizeof(payload), frame, sizeof(frame),
                             &frame_length) == NOSTOS_OK);

    nostos_result_t result = NOSTOS_EMPTY;
    for (size_t i = 0U; i < frame_length; ++i) {
        result = nostos_uart_feed(&parser, frame[i], (uint32_t)i,
                                  decoded, &decoded_length);
    }
    CHECK(result == NOSTOS_OK);
    CHECK(decoded_length == sizeof(payload));
    CHECK(memcmp(decoded, payload, sizeof(payload)) == 0);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"decode wheel only", test_decode_wheel_only},
        {"decode wheel and crank", test_decode_wheel_and_crank},
        {"reject bad length", test_decode_rejects_bad_length},
        {"cadence only", test_cadence_only_has_no_speed},
        {"speed rounds to 76", test_speed_example_rounds_to_76},
        {"event time wrap", test_event_time_wraparound},
        {"revolution wrap", test_revolution_wraparound},
        {"zero delta", test_zero_delta_does_not_advance_baseline},
        {"reset baseline", test_reset_requires_new_baseline},
        {"out of range", test_out_of_range_is_invalid},
        {"local payload roundtrip", test_local_payload_roundtrip},
        {"invalid local payload", test_invalid_local_payload_is_rejected},
        {"existing UART framer", test_local_payload_uses_existing_uart_framer}
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
