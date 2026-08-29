#ifndef XOSS_CSC_H
#define XOSS_CSC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    XOSS_CSC_OK = 0,
    XOSS_CSC_BASELINE,
    XOSS_CSC_NO_UPDATE,
    XOSS_CSC_NO_WHEEL_DATA,
    XOSS_CSC_BAD_ARGUMENT,
    XOSS_CSC_BAD_LENGTH,
    XOSS_CSC_OUT_OF_RANGE
} xoss_csc_result_t;

typedef struct {
    bool has_wheel;
    uint32_t cumulative_wheel_revolutions;
    uint16_t last_wheel_event_time_1024;
    bool has_crank;
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time_1024;
} xoss_csc_measurement_t;

typedef struct {
    bool has_baseline;
    uint32_t previous_wheel_revolutions;
    uint16_t previous_wheel_event_time_1024;
} xoss_csc_speed_state_t;

typedef struct {
    bool valid;
    uint16_t kmh_x10;
} xoss_speed_sample_t;

/* Decode Bluetooth CSC Measurement (0x2A5B). */
xoss_csc_result_t xoss_csc_decode(const uint8_t *payload,
                                  size_t length,
                                  xoss_csc_measurement_t *measurement);

/*
 * Calculate speed without floating point. The first wheel measurement becomes
 * a baseline and returns XOSS_CSC_BASELINE without producing a valid sample.
 */
xoss_csc_result_t xoss_csc_speed_update(xoss_csc_speed_state_t *state,
                                        const xoss_csc_measurement_t *measurement,
                                        uint16_t wheel_circumference_mm,
                                        xoss_speed_sample_t *sample);

void xoss_csc_speed_reset(xoss_csc_speed_state_t *state);
const char *xoss_csc_result_name(xoss_csc_result_t result);

#endif
