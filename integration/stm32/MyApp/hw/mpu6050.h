#ifndef MPU6050_H
#define MPU6050_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t temperature_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} mpu6050_data_t;

bool mpu6050_init(I2C_HandleTypeDef *i2c);
bool mpu6050_read(mpu6050_data_t *data);
bool mpu6050_is_ready(void);
uint8_t mpu6050_get_address(void);

#endif /* MPU6050_H */
