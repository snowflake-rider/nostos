#include "shared_state.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* Release의 NDEBUG 설정과 관계없이 항상 검사합니다. */
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void initially_missing(void)
{
    shared_state_t state;
    shared_sensor_value_t value;
    CHECK(shared_state_init(&state, 5000U));
    for (unsigned node = 0; node < SHARED_NODE_COUNT; ++node)
    {
        for (unsigned sensor = 0; sensor < SHARED_SENSOR_COUNT; ++sensor)
        {
            CHECK(shared_state_get(&state, (shared_node_t)node, (shared_sensor_t)sensor,
                                   0U, &value) == SHARED_VALUE_MISSING);
            CHECK(!value.has_value);
        }
    }
    puts("PASS initial value is missing, not a measured zero");
}

static void nodes_and_sensors_are_independent(void)
{
    shared_state_t state;
    shared_sensor_value_t value;
    CHECK(shared_state_init(&state, 5000U));
    CHECK(shared_state_update(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH, 23.5f, 100U));
    CHECK(shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, 28.0f, 200U));
    CHECK(shared_state_update(&state, SHARED_NODE_C, SHARED_SENSOR_TILT_DEG, -5.0f, 300U));
    CHECK(shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, 0.0f, 400U));

    CHECK(shared_state_get(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           401U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.has_value && value.value == 0.0f && value.updated_ms == 400U);
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           401U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.value == 23.5f && value.updated_ms == 100U);
    CHECK(shared_state_get(&state, SHARED_NODE_C, SHARED_SENSOR_TILT_DEG,
                           401U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.value == -5.0f && value.updated_ms == 300U);
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_TEMPERATURE_C,
                           401U, &value) == SHARED_VALUE_MISSING);
    puts("PASS source/sensor isolation, latest value, real zero and negative values");
}

static void stale_boundary_and_recovery(void)
{
    shared_state_t state;
    shared_sensor_value_t value;
    CHECK(shared_state_init(&state, 5000U));
    CHECK(shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, 28.0f, 1000U));
    CHECK(shared_state_get(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           5999U, &value) == SHARED_VALUE_FRESH);
    CHECK(shared_state_get(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           6000U, &value) == SHARED_VALUE_STALE);
    CHECK(value.has_value && value.value == 28.0f && value.updated_ms == 1000U);
    CHECK(shared_state_get(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           7000U, &value) == SHARED_VALUE_STALE);
    /* 같은 값이어도 새로 받았다면 갱신 시각을 바꿉니다. */
    CHECK(shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, 28.0f, 7000U));
    CHECK(shared_state_get(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           7001U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.updated_ms == 7000U);
    CHECK(shared_state_get(&state, SHARED_NODE_C, SHARED_SENSOR_SPEED_KMH,
                           7001U, &value) == SHARED_VALUE_MISSING);
    puts("PASS freshness boundary, retain stale value, same-value refresh/recovery");
}

static void invalid_values_keep_last_good_sample(void)
{
    shared_state_t state;
    shared_sensor_value_t value;
    CHECK(shared_state_init(&state, 100U));
    CHECK(shared_state_update(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH, 20.0f, 1000U));
    const float invalid[] = {NAN, INFINITY, -INFINITY};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        CHECK(!shared_state_update(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH, invalid[i], 1050U));
        CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                               1100U, &value) == SHARED_VALUE_STALE);
        CHECK(value.value == 20.0f && value.updated_ms == 1000U);
    }
    CHECK(!shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, NAN, 1050U));
    CHECK(shared_state_get(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           1100U, &value) == SHARED_VALUE_MISSING);
    puts("PASS NaN/infinity rejected without changing last value or freshness");
}

static void tick_wraparound(void)
{
    shared_state_t state;
    shared_sensor_value_t value;
    CHECK(shared_state_init(&state, 100U));
    CHECK(shared_state_update(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                              20.0f, UINT32_MAX - 49U));
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           0U, &value) == SHARED_VALUE_FRESH); /* 50ms */
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           49U, &value) == SHARED_VALUE_FRESH); /* 99ms */
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           50U, &value) == SHARED_VALUE_STALE); /* 100ms */
    CHECK(shared_state_update(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH, 21.0f, 51U));
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           52U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.value == 21.0f && value.updated_ms == 51U);
    puts("PASS 32-bit tick wraparound and update after wrap");
}

static void invalid_arguments_are_safe(void)
{
    shared_state_t state;
    shared_sensor_value_t value;
    CHECK(!shared_state_init(NULL, 100U));
    CHECK(shared_state_init(&state, 100U));
    CHECK(shared_state_update(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH, 20.0f, 0U));
    CHECK(!shared_state_init(&state, 0U)); /* failed init leaves the old data intact */
    CHECK(!shared_state_update(NULL, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH, 1.0f, 1U));

    const shared_node_t bad_nodes[] = {(shared_node_t)-1, SHARED_NODE_COUNT, (shared_node_t)INT_MAX};
    const shared_sensor_t bad_sensors[] = {(shared_sensor_t)-1, SHARED_SENSOR_COUNT, (shared_sensor_t)INT_MAX};
    for (size_t i = 0; i < sizeof(bad_nodes) / sizeof(bad_nodes[0]); ++i)
    {
        value = (shared_sensor_value_t){.value = 77.0f, .has_value = true, .updated_ms = 77U};
        CHECK(!shared_state_update(&state, bad_nodes[i], SHARED_SENSOR_SPEED_KMH, 1.0f, 1U));
        CHECK(shared_state_get(&state, bad_nodes[i], SHARED_SENSOR_SPEED_KMH,
                               1U, &value) == SHARED_VALUE_INVALID_ARGUMENT);
        CHECK(value.value == 77.0f && value.has_value && value.updated_ms == 77U);
    }
    for (size_t i = 0; i < sizeof(bad_sensors) / sizeof(bad_sensors[0]); ++i)
    {
        CHECK(!shared_state_update(&state, SHARED_NODE_A, bad_sensors[i], 1.0f, 1U));
        CHECK(shared_state_get(&state, SHARED_NODE_A, bad_sensors[i],
                               1U, &value) == SHARED_VALUE_INVALID_ARGUMENT);
        CHECK(value.value == 77.0f && value.has_value && value.updated_ms == 77U);
    }
    CHECK(shared_state_get(NULL, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           1U, &value) == SHARED_VALUE_INVALID_ARGUMENT);
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           1U, NULL) == SHARED_VALUE_INVALID_ARGUMENT);
    CHECK(value.value == 77.0f && value.has_value && value.updated_ms == 77U);
    CHECK(shared_state_get(&state, SHARED_NODE_A, SHARED_SENSOR_SPEED_KMH,
                           1U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.value == 20.0f && value.updated_ms == 0U); /* tick zero is valid */
    puts("PASS NULL/out-of-range IDs rejected; state and output remain unchanged");
}

static void snapshots_instances_and_reset(void)
{
    shared_state_t a, b;
    shared_sensor_value_t value;
    CHECK(shared_state_init(&a, 100U));
    CHECK(shared_state_init(&b, 100U));
    CHECK(shared_state_update(&a, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, 28.0f, 10U));
    CHECK(shared_state_get(&a, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           11U, &value) == SHARED_VALUE_FRESH);
    value.value = 99.0f;
    value.has_value = false;
    CHECK(shared_state_get(&a, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           11U, &value) == SHARED_VALUE_FRESH);
    CHECK(value.value == 28.0f && value.has_value);
    CHECK(shared_state_get(&b, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           11U, &value) == SHARED_VALUE_MISSING);
    CHECK(shared_state_init(&a, 50U));
    CHECK(shared_state_get(&a, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                           11U, &value) == SHARED_VALUE_MISSING);
    CHECK(!value.has_value);
    puts("PASS copied readings, independent instances, reset to missing");
}

int main(void)
{
    initially_missing();
    nodes_and_sensors_are_independent();
    stale_boundary_and_recovery();
    invalid_values_keep_last_good_sample();
    tick_wraparound();
    invalid_arguments_are_safe();
    snapshots_instances_and_reset();
    return 0;
}
