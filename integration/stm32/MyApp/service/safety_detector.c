#include "safety_detector.h"

#include <math.h>

#define DISTANCE_FILTER_SIZE 5U
#define DISTANCE_MIN_VALID_CM 2.0f
#define DISTANCE_MAX_VALID_CM 350.0f
#define DISTANCE_WARNING_CM 50.0f
#define DISTANCE_SAFE_CM 60.0f

#define FALL_IMPACT_THRESHOLD_G 1.5f
#define FALL_TILT_Z_THRESHOLD_G 0.55f
#define FALL_TILT_XY_THRESHOLD_G 0.70f
#define FALL_STILL_ROTATION_DPS 100.0f
#define FALL_OBSERVE_TIME_MS 1000U
#define FALL_IMPACT_TIMEOUT_MS 3000U
#define FALL_COUNTDOWN_TIME_MS 10000U

static safety_detector_status_t detector_status;
static float distance_history[DISTANCE_FILTER_SIZE];
static uint8_t distance_index = 0U;
static uint8_t distance_count = 0U;
static float filtered_distance = 0.0f;
static bool rear_warning_active = false;
static uint32_t fall_impact_tick = 0U;
static uint32_t fall_countdown_tick = 0U;

static void sort_distances(float *values, uint8_t count)
{
    for (uint8_t first = 0U; first < count; ++first)
    {
        for (uint8_t second = (uint8_t)(first + 1U); second < count; ++second)
        {
            if (values[first] > values[second])
            {
                float temporary = values[first];
                values[first] = values[second];
                values[second] = temporary;
            }
        }
    }
}

static float filter_distance(float new_distance, bool valid)
{
    if (!valid || (new_distance < DISTANCE_MIN_VALID_CM) ||
        (new_distance > DISTANCE_MAX_VALID_CM))
    {
        return filtered_distance;
    }

    distance_history[distance_index++] = new_distance;
    ++distance_count;

    if (filtered_distance <= 0.0f)
    {
        filtered_distance = new_distance;
    }

    if (distance_count >= DISTANCE_FILTER_SIZE)
    {
        float sorted[DISTANCE_FILTER_SIZE];
        for (uint8_t index = 0U; index < DISTANCE_FILTER_SIZE; ++index)
        {
            sorted[index] = distance_history[index];
        }

        sort_distances(sorted, DISTANCE_FILTER_SIZE);
        filtered_distance = sorted[DISTANCE_FILTER_SIZE / 2U];
        distance_index = 0U;
        distance_count = 0U;
    }

    return filtered_distance;
}

static safety_event_t evaluate_distance(float distance_cm)
{
    if (distance_cm <= 0.0f)
    {
        return SAFETY_EVENT_NONE;
    }

    if (distance_cm <= DISTANCE_WARNING_CM)
    {
        rear_warning_active = true;
    }
    else if (distance_cm >= DISTANCE_SAFE_CM)
    {
        rear_warning_active = false;
    }

    return rear_warning_active ?
        SAFETY_EVENT_REAR_WARNING : SAFETY_EVENT_REAR_SAFE;
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

    float total_acceleration =
        fabsf(accel_x) + fabsf(accel_y) + fabsf(accel_z);
    float total_rotation =
        fabsf(gyro_x) + fabsf(gyro_y) + fabsf(gyro_z);
    bool tilted =
        (fabsf(accel_z) < FALL_TILT_Z_THRESHOLD_G) ||
        (fabsf(accel_x) > FALL_TILT_XY_THRESHOLD_G) ||
        (fabsf(accel_y) > FALL_TILT_XY_THRESHOLD_G);
    bool still = total_rotation < FALL_STILL_ROTATION_DPS;

    detector_status.total_acceleration_g = total_acceleration;
    detector_status.total_rotation_dps = total_rotation;

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
    filtered_distance = 0.0f;
    rear_warning_active = false;
    distance_index = 0U;
    distance_count = 0U;
    fall_impact_tick = 0U;
    fall_countdown_tick = 0U;

    for (uint8_t index = 0U; index < DISTANCE_FILTER_SIZE; ++index)
    {
        distance_history[index] = 0.0f;
    }
}

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
)
{
    detector_status.current_distance_cm =
        filter_distance(distance_cm, distance_valid);

    safety_event_t event = evaluate_fall(
        current_tick,
        mpu_valid,
        accel_x,
        accel_y,
        accel_z,
        gyro_x,
        gyro_y,
        gyro_z);

    if (event == SAFETY_EVENT_NONE)
    {
        event = evaluate_distance(detector_status.current_distance_cm);
    }

    detector_status.current_event = event;
    return event;
}

message_type_t safety_detector_to_message(safety_event_t event)
{
    switch (event)
    {
        case SAFETY_EVENT_REAR_SAFE:
            return MSG_REAR_SAFE;
        case SAFETY_EVENT_REAR_WARNING:
            return MSG_REAR_WARNING;
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
