#ifndef SAFETY_SERVICE_H
#define SAFETY_SERVICE_H

#include "safety_detector.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool mpu_ready;
    bool mpu_data_valid;
    uint8_t mpu_address;
    uint32_t mpu_failure_count;
    bool distance_valid;
    float distance_cm;
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
} safety_service_status_t;

void safety_service_init(I2C_HandleTypeDef *i2c);
void safety_service_set_log_uart(UART_HandleTypeDef *uart);
bool safety_service_start_calibration(void);
bool safety_service_take_calibration_completed(void);
void safety_service_process(void);
const safety_service_status_t *safety_service_get_status(void);

#endif /* SAFETY_SERVICE_H */
