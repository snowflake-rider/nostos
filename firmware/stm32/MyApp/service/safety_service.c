#include "safety_service.h"

#include "app_config.h"
#include "message_router.h"
#include "mpu6050.h"
#include <stdio.h>
#if NOSTOS_PROTOCOL_V2
#include "message_protocol_service.h"
#endif
#if FEATURE_ULTRASONIC_SENSOR
#include "ultrasonic.h"
#endif

#define MPU_SAMPLE_PERIOD_MS 50U
#define MPU_RETRY_PERIOD_MS 1000U
#define MPU_MAX_CONSECUTIVE_FAILURES 10U
#define MPU_CALIBRATION_TIMEOUT_MS 10000U
#define MPU_LOG_TIMEOUT_MS 100U
#define ULTRASONIC_SAMPLE_PERIOD_MS 100U
#define ULTRASONIC_NO_ECHO_SAFE_COUNT 3U
#define ULTRASONIC_SAFE_DISTANCE_CM 100.0f

static I2C_HandleTypeDef *sensor_i2c = NULL;
static UART_HandleTypeDef *log_uart = NULL;
static mpu6050_data_t mpu_data;
static safety_service_status_t service_status;
static uint32_t mpu_sample_tick = 0U;
static uint32_t mpu_retry_tick = 0U;
static uint32_t calibration_started_tick = 0U;
static bool calibration_completed_pending = false;
#if FEATURE_ULTRASONIC_SENSOR
static uint32_t ultrasonic_sample_tick = 0U;
static uint8_t ultrasonic_no_echo_count = 0U;
static bool distance_sample_pending = false;
#if NOSTOS_PROTOCOL_V2
static bool rear_fault_reported;
#endif
#endif
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
}

void safety_service_init(I2C_HandleTypeDef *i2c)
{
    service_status = (safety_service_status_t){0};
    mpu_data = (mpu6050_data_t){0};
    sensor_i2c = i2c;
    log_uart = NULL;
    safety_detector_init();

#if FEATURE_FALL_DETECTION
    service_status.mpu_ready = mpu6050_init(sensor_i2c);
#endif

#if FEATURE_ULTRASONIC_SENSOR
    ultrasonic_init();
#endif

    uint32_t now = HAL_GetTick();
    mpu_sample_tick = now;
    mpu_retry_tick = now;
    calibration_started_tick = now;
    calibration_completed_pending = false;
#if FEATURE_ULTRASONIC_SENSOR
    ultrasonic_sample_tick = now;
    ultrasonic_no_echo_count = 0U;
    distance_sample_pending = false;
#if NOSTOS_PROTOCOL_V2
    rear_fault_reported=false;
#endif
#endif
    last_event = SAFETY_EVENT_NONE;
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
    calibration_completed_pending = false;
    safety_service_log_text("CAL_START\r\n");
    safety_service_update_debug_status();
    return true;
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

#if FEATURE_ULTRASONIC_SENSOR
    if ((uint32_t)(now - ultrasonic_sample_tick) >= ULTRASONIC_SAMPLE_PERIOD_MS)
    {
        ultrasonic_sample_tick = now;
        service_status.distance_valid =
            ultrasonic_read(&service_status.distance_cm);

        if (service_status.distance_valid)
        {
            ultrasonic_no_echo_count = 0U;
#if NOSTOS_PROTOCOL_V2
            if(rear_fault_reported && (last_event==SAFETY_EVENT_REAR_SAFE || last_event==SAFETY_EVENT_REAR_WARNING))
                last_event=SAFETY_EVENT_NONE;
            rear_fault_reported=false;
#endif
        }
        else if (ultrasonic_no_echo_count < ULTRASONIC_NO_ECHO_SAFE_COUNT)
        {
            ++ultrasonic_no_echo_count;
        }

        if (ultrasonic_no_echo_count >= ULTRASONIC_NO_ECHO_SAFE_COUNT)
        {
#if NOSTOS_PROTOCOL_V2
            /* Missing echo is unknown, never fabricated 100cm / SAFE. */
            if(!rear_fault_reported) {
                (void)message_protocol_service_publish_event(NOSTOS_REAR_UNKNOWN);
                rear_fault_reported=true;
            }
#else
            service_status.distance_cm = ULTRASONIC_SAFE_DISTANCE_CM;
            service_status.distance_valid = true;
#endif
        }

        distance_sample_pending = true;
    }
#endif

#if FEATURE_FALL_DETECTION
    if (!service_status.mpu_ready &&
        ((uint32_t)(now - mpu_retry_tick) >= MPU_RETRY_PERIOD_MS))
    {
        mpu_retry_tick = now;
        service_status.mpu_ready = mpu6050_init(sensor_i2c);
        if (service_status.mpu_ready)
        {
            service_status.mpu_failure_count = 0U;
        }
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
        if (service_status.mpu_data_valid)
        {
            service_status.mpu_failure_count = 0U;
        }
        else if (++service_status.mpu_failure_count >= MPU_MAX_CONSECUTIVE_FAILURES)
        {
            service_status.mpu_ready = false;
            service_status.mpu_failure_count = 0U;
            mpu_retry_tick = now;
        }
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
             MPU_CALIBRATION_TIMEOUT_MS))
        {
            safety_detector_fail_calibration(
                SAFETY_CALIBRATION_FAILED_UNSTABLE);
            detector = safety_detector_get_status();
        }

        if (detector->calibration_state != previous_calibration_state)
        {
            safety_service_log_calibration_result(
                detector->calibration_state);
            if (detector->calibration_state == SAFETY_CALIBRATION_READY)
            {
                calibration_completed_pending = true;
            }
        }
    }

    safety_event_t event = safety_detector_check(
        now,
#if FEATURE_ULTRASONIC_SENSOR
        distance_sample_pending && service_status.distance_valid,
        service_status.distance_cm,
#else
        false,
        0.0f,
#endif
        service_status.mpu_data_valid,
        mpu_data.accel_x,
        mpu_data.accel_y,
        mpu_data.accel_z,
        mpu_data.gyro_x,
        mpu_data.gyro_y,
        mpu_data.gyro_z);

#if FEATURE_ULTRASONIC_SENSOR
    distance_sample_pending = false;
#endif

    if (event != last_event)
    {
        last_event = event;
        message_type_t message = safety_detector_to_message(event);
#if NOSTOS_PROTOCOL_V2
        if((message==MSG_REAR_SAFE || message==MSG_REAR_WARNING) && !service_status.distance_valid)
            message=MSG_NONE;
#endif
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
