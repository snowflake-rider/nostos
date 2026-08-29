#ifndef SAFETY_DETECTOR_H
#define SAFETY_DETECTOR_H

#include "message_type.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SAFETY_EVENT_NONE = 0,
    SAFETY_EVENT_REAR_SAFE,
    SAFETY_EVENT_REAR_WARNING,
    SAFETY_EVENT_FALL_COUNTDOWN,
    SAFETY_EVENT_FALL_DETECTED,
} safety_event_t;

typedef enum
{
    FALL_STATE_IDLE = 0,
    FALL_STATE_IMPACT_DETECTED,
    FALL_STATE_COUNTDOWN,
    FALL_STATE_DETECTED,
} fall_state_t;

typedef enum
{
    SAFETY_CALIBRATION_UNCALIBRATED = 0,
    SAFETY_CALIBRATION_COLLECTING,
    SAFETY_CALIBRATION_READY,
    SAFETY_CALIBRATION_FAILED_SENSOR,
    SAFETY_CALIBRATION_FAILED_UNSTABLE,
} safety_calibration_state_t;

#define SAFETY_CALIBRATION_REQUIRED_SAMPLES 40U

typedef struct
{
    safety_event_t current_event;
    float current_distance_cm;
    float total_acceleration_g;
    float total_rotation_dps;
    float tilt_cosine;
    fall_state_t fall_state;
    uint32_t countdown_remaining_seconds;
    safety_calibration_state_t calibration_state;
    bool calibration_valid;
    uint32_t calibration_sample_count;
    float baseline_accel_x;
    float baseline_accel_y;
    float baseline_accel_z;
    float gyro_offset_x;
    float gyro_offset_y;
    float gyro_offset_z;
} safety_detector_status_t;

void safety_detector_init(void);
bool safety_detector_start_calibration(void);
bool safety_detector_add_calibration_sample(
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
);
void safety_detector_fail_calibration(safety_calibration_state_t failure_state);
safety_event_t safety_detector_check(
    uint32_t current_tick,
    bool distance_valid,
    float distance_cm,
    bool mpu_valid,
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
);
message_type_t safety_detector_to_message(safety_event_t event);
const safety_detector_status_t *safety_detector_get_status(void);

#endif /* SAFETY_DETECTOR_H */
