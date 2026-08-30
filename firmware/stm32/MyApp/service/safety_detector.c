#include "safety_detector.h"

#include <math.h>

#define FALL_IMPACT_THRESHOLD_G 1.5f
#define FALL_TILT_COSINE_THRESHOLD 0.70710678f
#define FALL_STILL_ROTATION_DPS 100.0f
#define FALL_OBSERVE_TIME_MS 1000U
#define FALL_IMPACT_TIMEOUT_MS 3000U
#define FALL_COUNTDOWN_TIME_MS 10000U

#define CALIBRATION_ACCEL_MIN_G 0.85f
#define CALIBRATION_ACCEL_MAX_G 1.15f
#define CALIBRATION_GYRO_MAX_DPS 5.0f

static safety_detector_status_t detector_status;
static uint32_t fall_impact_tick = 0U;
static uint32_t fall_countdown_tick = 0U;
static float calibration_accel_x_sum = 0.0f;
static float calibration_accel_y_sum = 0.0f;
static float calibration_accel_z_sum = 0.0f;
static float calibration_gyro_x_sum = 0.0f;
static float calibration_gyro_y_sum = 0.0f;
static float calibration_gyro_z_sum = 0.0f;

static float vector_magnitude(float x, float y, float z)
{
    return sqrtf((x * x) + (y * y) + (z * z));
}

static void reset_calibration_accumulator(void)
{
    calibration_accel_x_sum = 0.0f;
    calibration_accel_y_sum = 0.0f;
    calibration_accel_z_sum = 0.0f;
    calibration_gyro_x_sum = 0.0f;
    calibration_gyro_y_sum = 0.0f;
    calibration_gyro_z_sum = 0.0f;
    detector_status.calibration_sample_count = 0U;
}

static safety_event_t continue_fall_countdown(uint32_t current_tick)
{
    uint32_t elapsed = current_tick - fall_countdown_tick;
    if (elapsed >= FALL_COUNTDOWN_TIME_MS)
    {
        detector_status.fall_state = FALL_STATE_DETECTED;
        detector_status.countdown_remaining_seconds = 0U;
        return SAFETY_EVENT_FALL_DETECTED;
    }

    detector_status.countdown_remaining_seconds =
        (FALL_COUNTDOWN_TIME_MS - elapsed + 999U) / 1000U;
    return SAFETY_EVENT_FALL_COUNTDOWN;
}

static safety_event_t evaluate_fall(
    uint32_t current_tick,
    bool valid,
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
)
{
    if (!valid)
    {
        if (detector_status.fall_state == FALL_STATE_COUNTDOWN)
        {
            return continue_fall_countdown(current_tick);
        }
        if (detector_status.fall_state == FALL_STATE_DETECTED)
        {
            return SAFETY_EVENT_FALL_DETECTED;
        }
        return SAFETY_EVENT_NONE;
    }

    float corrected_gyro_x = gyro_x - detector_status.gyro_offset_x;
    float corrected_gyro_y = gyro_y - detector_status.gyro_offset_y;
    float corrected_gyro_z = gyro_z - detector_status.gyro_offset_z;
    float total_acceleration = vector_magnitude(accel_x, accel_y, accel_z);
    float total_rotation = vector_magnitude(
        corrected_gyro_x,
        corrected_gyro_y,
        corrected_gyro_z);
    float tilt_cosine = 1.0f;

    if (total_acceleration > 0.01f)
    {
        tilt_cosine =
            ((accel_x * detector_status.baseline_accel_x) +
             (accel_y * detector_status.baseline_accel_y) +
             (accel_z * detector_status.baseline_accel_z)) /
            total_acceleration;
        if (tilt_cosine > 1.0f)
        {
            tilt_cosine = 1.0f;
        }
        else if (tilt_cosine < -1.0f)
        {
            tilt_cosine = -1.0f;
        }
    }

    bool tilted = tilt_cosine < FALL_TILT_COSINE_THRESHOLD;
    bool still = total_rotation < FALL_STILL_ROTATION_DPS;

    detector_status.total_acceleration_g = total_acceleration;
    detector_status.total_rotation_dps = total_rotation;
    detector_status.tilt_cosine = tilt_cosine;

    switch (detector_status.fall_state)
    {
        case FALL_STATE_IDLE:
            if (total_acceleration >= FALL_IMPACT_THRESHOLD_G)
            {
                detector_status.fall_state = FALL_STATE_IMPACT_DETECTED;
                fall_impact_tick = current_tick;
            }
            break;

        case FALL_STATE_IMPACT_DETECTED:
        {
            uint32_t elapsed = current_tick - fall_impact_tick;
            if ((elapsed >= FALL_OBSERVE_TIME_MS) && tilted && still)
            {
                detector_status.fall_state = FALL_STATE_COUNTDOWN;
                fall_countdown_tick = current_tick;
                detector_status.countdown_remaining_seconds = 10U;
                return SAFETY_EVENT_FALL_COUNTDOWN;
            }
            if (elapsed >= FALL_IMPACT_TIMEOUT_MS)
            {
                detector_status.fall_state = FALL_STATE_IDLE;
            }
            break;
        }

        case FALL_STATE_COUNTDOWN:
            return continue_fall_countdown(current_tick);

        case FALL_STATE_DETECTED:
            return SAFETY_EVENT_FALL_DETECTED;

        default:
            detector_status.fall_state = FALL_STATE_IDLE;
            break;
    }

    return SAFETY_EVENT_NONE;
}

void safety_detector_init(void)
{
    detector_status = (safety_detector_status_t){0};
    detector_status.total_acceleration_g = 1.0f;
    detector_status.tilt_cosine = 1.0f;
    detector_status.calibration_state = SAFETY_CALIBRATION_UNCALIBRATED;
    fall_impact_tick = 0U;
    fall_countdown_tick = 0U;
    reset_calibration_accumulator();
}

bool safety_detector_start_calibration(void)
{
    if (detector_status.fall_state != FALL_STATE_IDLE)
    {
        return false;
    }

    reset_calibration_accumulator();
    detector_status.calibration_state = SAFETY_CALIBRATION_COLLECTING;
    return true;
}

bool safety_detector_add_calibration_sample(
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
)
{
    if (detector_status.calibration_state != SAFETY_CALIBRATION_COLLECTING)
    {
        return false;
    }

    float acceleration = vector_magnitude(accel_x, accel_y, accel_z);
    float rotation = vector_magnitude(gyro_x, gyro_y, gyro_z);
    bool finite_sample = isfinite(accel_x) && isfinite(accel_y) &&
                         isfinite(accel_z) && isfinite(gyro_x) &&
                         isfinite(gyro_y) && isfinite(gyro_z);

    if (!finite_sample || (acceleration < CALIBRATION_ACCEL_MIN_G) ||
        (acceleration > CALIBRATION_ACCEL_MAX_G) ||
        (rotation > CALIBRATION_GYRO_MAX_DPS))
    {
        /* 연속으로 안정된 40개를 요구하므로 움직이면 처음부터 다시 모읍니다. */
        reset_calibration_accumulator();
        return false;
    }

    calibration_accel_x_sum += accel_x;
    calibration_accel_y_sum += accel_y;
    calibration_accel_z_sum += accel_z;
    calibration_gyro_x_sum += gyro_x;
    calibration_gyro_y_sum += gyro_y;
    calibration_gyro_z_sum += gyro_z;
    ++detector_status.calibration_sample_count;

    if (detector_status.calibration_sample_count <
        SAFETY_CALIBRATION_REQUIRED_SAMPLES)
    {
        return false;
    }

    const float count = (float)SAFETY_CALIBRATION_REQUIRED_SAMPLES;
    float baseline_x = calibration_accel_x_sum / count;
    float baseline_y = calibration_accel_y_sum / count;
    float baseline_z = calibration_accel_z_sum / count;
    float baseline_magnitude = vector_magnitude(
        baseline_x,
        baseline_y,
        baseline_z);

    if (!isfinite(baseline_magnitude) || (baseline_magnitude < 0.01f))
    {
        reset_calibration_accumulator();
        return false;
    }

    detector_status.baseline_accel_x = baseline_x / baseline_magnitude;
    detector_status.baseline_accel_y = baseline_y / baseline_magnitude;
    detector_status.baseline_accel_z = baseline_z / baseline_magnitude;
    detector_status.gyro_offset_x = calibration_gyro_x_sum / count;
    detector_status.gyro_offset_y = calibration_gyro_y_sum / count;
    detector_status.gyro_offset_z = calibration_gyro_z_sum / count;
    detector_status.calibration_valid = true;
    detector_status.calibration_state = SAFETY_CALIBRATION_READY;
    return true;
}

void safety_detector_fail_calibration(safety_calibration_state_t failure_state)
{
    if ((failure_state != SAFETY_CALIBRATION_FAILED_SENSOR) &&
        (failure_state != SAFETY_CALIBRATION_FAILED_UNSTABLE))
    {
        return;
    }

    reset_calibration_accumulator();
    detector_status.calibration_state = failure_state;
}

safety_event_t safety_detector_check(
    uint32_t current_tick,
    bool mpu_valid,
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
)
{
    bool fall_sample_valid = mpu_valid && detector_status.calibration_valid &&
        (detector_status.calibration_state != SAFETY_CALIBRATION_COLLECTING);
    safety_event_t event = evaluate_fall(
        current_tick,
        fall_sample_valid,
        accel_x,
        accel_y,
        accel_z,
        gyro_x,
        gyro_y,
        gyro_z);

    detector_status.current_event = event;
    return event;
}

message_type_t safety_detector_to_message(safety_event_t event)
{
    switch (event)
    {
        case SAFETY_EVENT_FALL_DETECTED:
            return MSG_FALL_DETECTED;
        case SAFETY_EVENT_NONE:
        case SAFETY_EVENT_FALL_COUNTDOWN:
        default:
            return MSG_NONE;
    }
}

const safety_detector_status_t *safety_detector_get_status(void)
{
    return &detector_status;
}
