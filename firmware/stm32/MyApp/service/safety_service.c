#include "safety_service.h"

#include "app_config.h"
#include "message_router.h"
#include "mpu6050.h"
#include "sensor_store.h"
#include <stdio.h>
#define MPU_SAMPLE_PERIOD_MS 50U
#define MPU_RETRY_PERIOD_MS 1000U
#define MPU_MAX_CONSECUTIVE_FAILURES 10U
#define MPU_LOG_TIMEOUT_MS 100U
static I2C_HandleTypeDef *sensor_i2c = NULL;
static UART_HandleTypeDef *log_uart = NULL;
static mpu6050_data_t mpu_data;
static safety_service_status_t service_status;
static uint32_t mpu_sample_tick = 0U;
static uint32_t mpu_retry_tick = 0U;
static uint32_t calibration_started_tick = 0U;
static uint32_t cal_session_tick = 0U;
static uint32_t recalibration_hold_started_tick = 0U;
static bool recalibration_button_pressed = false;
static bool calibration_completed_pending = false;
static safety_event_t last_event = SAFETY_EVENT_NONE;

static long safety_service_to_milli(float value)
{
    float scaled = value * 1000.0f;
    return (long)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

static void safety_service_log_text(const char *text)
{
    if ((log_uart == NULL) || (text == NULL))
    {
        return;
    }

    size_t length = 0U;
    while ((text[length] != '\0') && (length < UINT16_MAX))
    {
        ++length;
    }

    if (length > 0U)
    {
        (void)HAL_UART_Transmit(
            log_uart,
            (uint8_t *)text,
            (uint16_t)length,
            MPU_LOG_TIMEOUT_MS);
    }
}

static void safety_service_log_calibration_result(
    safety_calibration_state_t state
)
{
    if (state == SAFETY_CALIBRATION_READY)
    {
        const safety_detector_status_t *detector = safety_detector_get_status();
        char line[160];
        int length = snprintf(
            line,
            sizeof(line),
            "CAL_OK,ax_mg=%ld,ay_mg=%ld,az_mg=%ld,"
            "gx_mdps=%ld,gy_mdps=%ld,gz_mdps=%ld\r\n",
            safety_service_to_milli(detector->baseline_accel_x),
            safety_service_to_milli(detector->baseline_accel_y),
            safety_service_to_milli(detector->baseline_accel_z),
            safety_service_to_milli(detector->gyro_offset_x),
            safety_service_to_milli(detector->gyro_offset_y),
            safety_service_to_milli(detector->gyro_offset_z));

        if ((length > 0) && ((size_t)length < sizeof(line)))
        {
            safety_service_log_text(line);
        }
    }
    else if (state == SAFETY_CALIBRATION_FAILED_SENSOR)
    {
        safety_service_log_text("CAL_FAIL,SENSOR\r\n");
    }
    else if (state == SAFETY_CALIBRATION_FAILED_UNSTABLE)
    {
        safety_service_log_text("CAL_FAIL,UNSTABLE\r\n");
    }
}

static void safety_service_update_debug_status(void)
{
    const safety_detector_status_t *detector = safety_detector_get_status();
    service_status.mpu_address = mpu6050_get_address();
    service_status.event = detector->current_event;
    service_status.fall_state = detector->fall_state;
    service_status.countdown_remaining_seconds =
        detector->countdown_remaining_seconds;
    service_status.calibration_state = detector->calibration_state;
    service_status.calibration_valid = detector->calibration_valid;
    service_status.calibration_sample_count = detector->calibration_sample_count;
    service_status.baseline_accel_x = detector->baseline_accel_x;
    service_status.baseline_accel_y = detector->baseline_accel_y;
    service_status.baseline_accel_z = detector->baseline_accel_z;
    service_status.gyro_offset_x = detector->gyro_offset_x;
    service_status.gyro_offset_y = detector->gyro_offset_y;
    service_status.gyro_offset_z = detector->gyro_offset_z;

    uint32_t progress = 0U;
    if (service_status.cal_session_state == SAFETY_CAL_SESSION_RUNNING)
    {
        progress = (detector->calibration_sample_count * 1000U) /
            SAFETY_CALIBRATION_REQUIRED_SAMPLES;
    }
    else if ((service_status.cal_session_state == SAFETY_CAL_SESSION_SUCCESS) ||
             (service_status.cal_session_state == SAFETY_CAL_SESSION_READY))
    {
        progress = 1000U;
    }
    else if (service_status.cal_session_state == SAFETY_CAL_SESSION_REQUIRED)
    {
        progress = (service_status.recalibration_hold_ms * 1000U) /
            SAFETY_RECALIBRATION_HOLD_MS;
    }
    if (progress > 1000U)
    {
        progress = 1000U;
    }
    service_status.calibration_progress_per_mille = (uint16_t)progress;
}

#if FEATURE_FALL_DETECTION
static void safety_service_begin_session(uint32_t now)
{
    safety_detector_init();
    service_status.mpu_ready = false;
    service_status.mpu_data_valid = false;
    service_status.mpu_failure_count = 0U;
    service_status.cal_session_state = SAFETY_CAL_SESSION_INIT;
    if (service_status.cal_session_id != UINT32_MAX)
    {
        ++service_status.cal_session_id;
    }
    service_status.recalibration_hold_ms = 0U;
    recalibration_button_pressed = false;
    recalibration_hold_started_tick = now;
    cal_session_tick = now;
    mpu_sample_tick = now;
    mpu_retry_tick = now - MPU_RETRY_PERIOD_MS;
    calibration_started_tick = now;
    calibration_completed_pending = false;
    last_event = SAFETY_EVENT_NONE;
}

static bool safety_service_fall_must_finish_without_sensor(void)
{
    fall_state_t state = safety_detector_get_status()->fall_state;
    return (state == FALL_STATE_COUNTDOWN) ||
        (state == FALL_STATE_DETECTED);
}
#endif

static void safety_service_require_recalibration(
    uint32_t now,
    safety_calibration_state_t failure_state)
{
    safety_detector_fail_calibration(failure_state);
    safety_service_log_calibration_result(failure_state);
    service_status.cal_session_state = SAFETY_CAL_SESSION_REQUIRED;
    service_status.recalibration_hold_ms = 0U;
    recalibration_button_pressed = false;
    recalibration_hold_started_tick = now;
    cal_session_tick = now;
    calibration_completed_pending = false;
}

void safety_service_init(I2C_HandleTypeDef *i2c)
{
    service_status = (safety_service_status_t){0};
    mpu_data = (mpu6050_data_t){0};
    sensor_i2c = i2c;
    log_uart = NULL;
    safety_detector_init();

    uint32_t now = HAL_GetTick();
    mpu_sample_tick = now;
    mpu_retry_tick = now - MPU_RETRY_PERIOD_MS;
    calibration_started_tick = now;
    cal_session_tick = now;
    recalibration_hold_started_tick = now;
    recalibration_button_pressed = false;
    calibration_completed_pending = false;
    last_event = SAFETY_EVENT_NONE;
#if FEATURE_FALL_DETECTION
    service_status.cal_session_state = SAFETY_CAL_SESSION_INIT;
    service_status.cal_session_id = 1U;
#else
    service_status.cal_session_state = SAFETY_CAL_SESSION_READY;
#endif
    safety_service_update_debug_status();
}

void safety_service_set_log_uart(UART_HandleTypeDef *uart)
{
    log_uart = uart;
}

bool safety_service_start_calibration(void)
{
    if (!service_status.mpu_ready || !safety_detector_start_calibration())
    {
        safety_service_log_text("CAL_REJECT,SENSOR_OR_FALL_STATE\r\n");
        safety_service_update_debug_status();
        return false;
    }

    calibration_started_tick = HAL_GetTick();
    cal_session_tick = calibration_started_tick;
    service_status.cal_session_state = SAFETY_CAL_SESSION_RUNNING;
    calibration_completed_pending = false;
    safety_service_log_text("CAL_START\r\n");
    safety_service_update_debug_status();
    return true;
}

void safety_service_set_recalibration_button(bool pressed)
{
    if (service_status.cal_session_state != SAFETY_CAL_SESSION_REQUIRED)
    {
        recalibration_button_pressed = false;
        service_status.recalibration_hold_ms = 0U;
        return;
    }

    if (pressed == recalibration_button_pressed)
    {
        return;
    }

    recalibration_button_pressed = pressed;
    service_status.recalibration_hold_ms = 0U;
    if (pressed)
    {
        recalibration_hold_started_tick = HAL_GetTick();
    }
}

bool safety_service_buttons_blocked(void)
{
    return service_status.cal_session_state != SAFETY_CAL_SESSION_READY;
}

bool safety_service_take_calibration_completed(void)
{
    bool completed = calibration_completed_pending;
    calibration_completed_pending = false;
    return completed;
}

void safety_service_process(void)
{
    uint32_t now = HAL_GetTick();

#if FEATURE_FALL_DETECTION
    if (service_status.cal_session_state == SAFETY_CAL_SESSION_INIT)
    {
        uint32_t init_elapsed = (uint32_t)(now - cal_session_tick);
        if (init_elapsed >= SAFETY_CAL_INIT_TIMEOUT_MS)
        {
            safety_service_require_recalibration(
                now, SAFETY_CALIBRATION_FAILED_SENSOR);
        }
        else if ((init_elapsed >= SAFETY_CAL_INIT_DISPLAY_MS) &&
                 ((uint32_t)(now - mpu_retry_tick) >= MPU_RETRY_PERIOD_MS))
        {
            mpu_retry_tick = now;
            service_status.mpu_ready = mpu6050_init(sensor_i2c);
            if (service_status.mpu_ready)
            {
                service_status.mpu_failure_count = 0U;
                (void)safety_service_start_calibration();
            }
        }
        safety_service_update_debug_status();
        return;
    }

    if (service_status.cal_session_state == SAFETY_CAL_SESSION_REQUIRED)
    {
        if (recalibration_button_pressed)
        {
            uint32_t held_ms =
                (uint32_t)(now - recalibration_hold_started_tick);
            service_status.recalibration_hold_ms = held_ms;
            if (held_ms >= SAFETY_RECALIBRATION_HOLD_MS)
            {
                safety_service_begin_session(now);
            }
        }
        safety_service_update_debug_status();
        return;
    }

    if (service_status.cal_session_state == SAFETY_CAL_SESSION_SUCCESS)
    {
        if ((uint32_t)(now - cal_session_tick) >=
            SAFETY_CAL_SUCCESS_DISPLAY_MS)
        {
            service_status.cal_session_state = SAFETY_CAL_SESSION_READY;
        }
        safety_service_update_debug_status();
        return;
    }
#endif

    if ((uint32_t)(now - mpu_sample_tick) < MPU_SAMPLE_PERIOD_MS)
    {
        safety_service_update_debug_status();
        return;
    }

    mpu_sample_tick = now;
    service_status.mpu_data_valid = false;

#if FEATURE_FALL_DETECTION
    if (service_status.mpu_ready)
    {
        service_status.mpu_data_valid = mpu6050_read(&mpu_data);
        (void)sensor_store_update_imu(
            service_status.mpu_data_valid,
            mpu_data.accel_x,
            mpu_data.accel_y,
            mpu_data.accel_z,
            mpu_data.gyro_x,
            mpu_data.gyro_y,
            mpu_data.gyro_z,
            now
        );
        if (service_status.mpu_data_valid)
        {
            service_status.mpu_failure_count = 0U;
        }
        else if (++service_status.mpu_failure_count >= MPU_MAX_CONSECUTIVE_FAILURES)
        {
            service_status.mpu_ready = false;
            service_status.mpu_failure_count = 0U;
            mpu_retry_tick = now;
            if (!safety_service_fall_must_finish_without_sensor())
            {
                safety_service_require_recalibration(
                    now, SAFETY_CALIBRATION_FAILED_SENSOR);
            }
        }
    }
    else
    {
        (void)sensor_store_update_imu(
            false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, now
        );
    }
#endif

#if FEATURE_FALL_DETECTION
    if (service_status.cal_session_state == SAFETY_CAL_SESSION_REQUIRED)
    {
        safety_service_update_debug_status();
        return;
    }
#endif

    const safety_detector_status_t *detector = safety_detector_get_status();
    safety_calibration_state_t previous_calibration_state =
        detector->calibration_state;

    if (previous_calibration_state == SAFETY_CALIBRATION_COLLECTING)
    {
        if (service_status.mpu_data_valid)
        {
            (void)safety_detector_add_calibration_sample(
                mpu_data.accel_x,
                mpu_data.accel_y,
                mpu_data.accel_z,
                mpu_data.gyro_x,
                mpu_data.gyro_y,
                mpu_data.gyro_z);
        }
        else if (!service_status.mpu_ready)
        {
            safety_detector_fail_calibration(
                SAFETY_CALIBRATION_FAILED_SENSOR);
        }

        detector = safety_detector_get_status();
        if ((detector->calibration_state == SAFETY_CALIBRATION_COLLECTING) &&
            ((uint32_t)(now - calibration_started_tick) >=
             SAFETY_CAL_RUNNING_TIMEOUT_MS))
        {
            safety_service_require_recalibration(
                now, SAFETY_CALIBRATION_FAILED_UNSTABLE);
            safety_service_update_debug_status();
            return;
        }

        if (detector->calibration_state != previous_calibration_state)
        {
            safety_service_log_calibration_result(
                detector->calibration_state);
            if (detector->calibration_state == SAFETY_CALIBRATION_READY)
            {
                calibration_completed_pending = true;
                service_status.cal_session_state = SAFETY_CAL_SESSION_SUCCESS;
                cal_session_tick = now;
            }
        }
    }

    safety_event_t event = SAFETY_EVENT_NONE;
    if (service_status.cal_session_state == SAFETY_CAL_SESSION_READY)
    {
        event = safety_detector_check(
            now,
            service_status.mpu_data_valid,
            mpu_data.accel_x,
            mpu_data.accel_y,
            mpu_data.accel_z,
            mpu_data.gyro_x,
            mpu_data.gyro_y,
            mpu_data.gyro_z);
    }

    if (event != last_event)
    {
        last_event = event;
        message_type_t message = safety_detector_to_message(event);
        if (message != MSG_NONE)
        {
            (void)message_router_publish_local(message);
        }
    }

    safety_service_update_debug_status();
}

const safety_service_status_t *safety_service_get_status(void)
{
    return &service_status;
}
