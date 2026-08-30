#ifndef NOSTOS_XOSS_SPEED_SENSOR_H
#define NOSTOS_XOSS_SPEED_SENSOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPEED_SENSOR_CSC_SERVICE_UUID 0x1816U
#define SPEED_SENSOR_CSC_MEASUREMENT_UUID 0x2A5BU
#define SPEED_SENSOR_CSC_FEATURE_UUID 0x2A5CU

typedef enum {
    SPEED_SENSOR_OK = 0,
    SPEED_SENSOR_BASELINE,
    SPEED_SENSOR_REBASELINE,
    SPEED_SENSOR_NO_UPDATE,
    SPEED_SENSOR_NO_WHEEL_DATA,
    SPEED_SENSOR_BAD_ARGUMENT,
    SPEED_SENSOR_BAD_FLAGS,
    SPEED_SENSOR_BAD_LENGTH,
    SPEED_SENSOR_OUT_OF_RANGE
} speed_sensor_result_t;

typedef struct {
    bool has_wheel;
    uint32_t cumulative_wheel_revolutions;
    uint16_t last_wheel_event_time_1024;
    bool has_crank;
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time_1024;
} speed_sensor_measurement_t;

typedef struct {
    uint64_t distance_mm;
    uint32_t previous_wheel_revolutions;
    uint16_t previous_wheel_event_time_1024;
    bool has_baseline;
} speed_sensor_state_t;

typedef struct {
    /* valid applies to the instantaneous speed fields. */
    bool valid;
    uint16_t kmh_x10;
    uint32_t delta_revolutions;
    /* Session distance since the first baseline; retained on rebaseline. */
    uint64_t distance_mm;
} speed_sensor_sample_t;

/* Decode one Bluetooth CSC Measurement (0x2A5B) notification. */
speed_sensor_result_t speed_sensor_decode_csc(
    const uint8_t *payload,
    size_t length,
    speed_sensor_measurement_t *measurement);

/*
 * Convert consecutive wheel measurements to km/h x10 and session distance
 * using integer math. The first sample establishes a baseline at distance 0
 * and does not produce a speed.
 */
speed_sensor_result_t speed_sensor_update(
    speed_sensor_state_t *state,
    const speed_sensor_measurement_t *measurement,
    uint32_t travel_circumference_mm,
    speed_sensor_sample_t *sample);

void speed_sensor_reset(speed_sensor_state_t *state);
/* Start a new wheel-event baseline while retaining session distance. */
void speed_sensor_rebaseline(speed_sensor_state_t *state);
const char *speed_sensor_result_name(speed_sensor_result_t result);

#endif
