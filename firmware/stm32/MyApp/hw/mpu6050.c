#include "mpu6050.h"

#include <stddef.h>

#define MPU6050_ADDRESS_LOW_7BIT 0x68U
#define MPU6050_ADDRESS_HIGH_7BIT 0x69U
#define MPU6050_REG_SAMPLE_RATE_DIVIDER 0x19U
#define MPU6050_REG_CONFIG 0x1AU
#define MPU6050_REG_GYRO_CONFIG 0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_POWER_MANAGEMENT_1 0x6BU
#define MPU6050_REG_WHO_AM_I 0x75U
#define MPU6050_I2C_TIMEOUT_MS 100U
#define MPU6050_READ_RETRY_COUNT 3U

static I2C_HandleTypeDef *mpu_i2c = NULL;
static uint16_t mpu_device_address = 0U;
static bool mpu_ready = false;

static bool mpu6050_find_address(void)
{
    static const uint8_t addresses[] = {
        MPU6050_ADDRESS_LOW_7BIT,
        MPU6050_ADDRESS_HIGH_7BIT,
    };

    for (uint32_t index = 0U; index < sizeof(addresses); ++index)
    {
        uint16_t hal_address = (uint16_t)(addresses[index] << 1U);
        if (HAL_I2C_IsDeviceReady(
                mpu_i2c,
                hal_address,
                3U,
                MPU6050_I2C_TIMEOUT_MS) == HAL_OK)
        {
            mpu_device_address = hal_address;
            return true;
        }
    }

    return false;
}

static bool mpu6050_write_register(uint8_t register_address, uint8_t value)
{
    return HAL_I2C_Mem_Write(
               mpu_i2c,
               mpu_device_address,
               register_address,
               I2C_MEMADD_SIZE_8BIT,
               &value,
               1U,
               MPU6050_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool mpu6050_read_registers(
    uint8_t register_address,
    uint8_t *buffer,
    uint16_t length
)
{
    for (uint32_t retry = 0U; retry < MPU6050_READ_RETRY_COUNT; ++retry)
    {
        if (HAL_I2C_Mem_Read(
                mpu_i2c,
                mpu_device_address,
                register_address,
                I2C_MEMADD_SIZE_8BIT,
                buffer,
                length,
                MPU6050_I2C_TIMEOUT_MS) == HAL_OK)
        {
            return true;
        }

        HAL_Delay(2U);
    }

    return false;
}

static bool mpu6050_valid_chip_id(uint8_t chip_id)
{
    return (chip_id == 0x68U) || (chip_id == 0x70U) ||
           (chip_id == 0x71U) || (chip_id == 0x72U) ||
           (chip_id == 0x73U);
}

bool mpu6050_init(I2C_HandleTypeDef *i2c)
{
    uint8_t chip_id = 0U;

    mpu_i2c = i2c;
    mpu_device_address = 0U;
    mpu_ready = false;

    if (mpu_i2c == NULL)
    {
        return false;
    }

    HAL_Delay(100U);
    if (!mpu6050_find_address() ||
        !mpu6050_read_registers(MPU6050_REG_WHO_AM_I, &chip_id, 1U) ||
        !mpu6050_valid_chip_id(chip_id))
    {
        return false;
    }

    if (!mpu6050_write_register(MPU6050_REG_POWER_MANAGEMENT_1, 0x80U))
    {
        return false;
    }

    HAL_Delay(100U);
    if (!mpu6050_write_register(MPU6050_REG_POWER_MANAGEMENT_1, 0x01U))
    {
        return false;
    }

    HAL_Delay(10U);
    if (!mpu6050_write_register(MPU6050_REG_CONFIG, 0x03U) ||
        !mpu6050_write_register(MPU6050_REG_SAMPLE_RATE_DIVIDER, 0x07U) ||
        !mpu6050_write_register(MPU6050_REG_GYRO_CONFIG, 0x00U) ||
        !mpu6050_write_register(MPU6050_REG_ACCEL_CONFIG, 0x00U))
    {
        return false;
    }

    mpu_ready = true;
    return true;
}

bool mpu6050_read(mpu6050_data_t *data)
{
    uint8_t buffer[14] = {0};

    if ((data == NULL) || !mpu_ready)
    {
        return false;
    }

    if (!mpu6050_read_registers(
            MPU6050_REG_ACCEL_XOUT_H,
            buffer,
            sizeof(buffer)))
    {
        return false;
    }

    data->accel_x_raw = (int16_t)(((uint16_t)buffer[0] << 8U) | buffer[1]);
    data->accel_y_raw = (int16_t)(((uint16_t)buffer[2] << 8U) | buffer[3]);
    data->accel_z_raw = (int16_t)(((uint16_t)buffer[4] << 8U) | buffer[5]);
    data->temperature_raw = (int16_t)(((uint16_t)buffer[6] << 8U) | buffer[7]);
    data->gyro_x_raw = (int16_t)(((uint16_t)buffer[8] << 8U) | buffer[9]);
    data->gyro_y_raw = (int16_t)(((uint16_t)buffer[10] << 8U) | buffer[11]);
    data->gyro_z_raw = (int16_t)(((uint16_t)buffer[12] << 8U) | buffer[13]);

    data->accel_x = (float)data->accel_x_raw / 16384.0f;
    data->accel_y = (float)data->accel_y_raw / 16384.0f;
    data->accel_z = (float)data->accel_z_raw / 16384.0f;
    data->gyro_x = (float)data->gyro_x_raw / 131.0f;
    data->gyro_y = (float)data->gyro_y_raw / 131.0f;
    data->gyro_z = (float)data->gyro_z_raw / 131.0f;

    return true;
}

bool mpu6050_is_ready(void)
{
    return mpu_ready;
}

uint8_t mpu6050_get_address(void)
{
    return (uint8_t)(mpu_device_address >> 1U);
}
