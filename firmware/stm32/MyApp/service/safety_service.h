#ifndef SAFETY_SERVICE_H
#define SAFETY_SERVICE_H

#include "safety_detector.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SAFETY_CAL_SESSION_INIT = 0,
    SAFETY_CAL_SESSION_RUNNING,
    SAFETY_CAL_SESSION_SUCCESS,
    SAFETY_CAL_SESSION_REQUIRED,
    SAFETY_CAL_SESSION_READY,
} safety_cal_session_state_t;

#define SAFETY_CAL_INIT_DISPLAY_MS 500U
#define SAFETY_CAL_INIT_TIMEOUT_MS 3000U
#define SAFETY_CAL_RUNNING_TIMEOUT_MS 8000U
#define SAFETY_CAL_SUCCESS_DISPLAY_MS 1000U
#define SAFETY_RECALIBRATION_HOLD_MS 3000U

typedef struct
{
    bool mpu_ready;
    bool mpu_data_valid;
    uint8_t mpu_address;
    uint32_t mpu_failure_count;
    safety_event_t event;
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
    safety_cal_session_state_t cal_session_state;
    uint32_t cal_session_id;
    uint16_t calibration_progress_per_mille;
    uint32_t recalibration_hold_ms;
} safety_service_status_t;

void safety_service_init(I2C_HandleTypeDef *i2c);
void safety_service_set_log_uart(UART_HandleTypeDef *uart);
bool safety_service_start_calibration(void);
bool safety_service_take_calibration_completed(void);
void safety_service_set_recalibration_button(bool pressed);
bool safety_service_buttons_blocked(void);
void safety_service_process(void);
const safety_service_status_t *safety_service_get_status(void);

#endif /* SAFETY_SERVICE_H */
