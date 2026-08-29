#include "safety_service.h"

#include "message_router.h"
#include "mpu6050.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static bool mpu_init_ok;
static bool mpu_read_ok;
static mpu6050_data_t next_sample;
static char log_output[512];
static size_t log_length;

uint32_t HAL_GetTick(void)
{
    return tick;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    return GPIO_PIN_SET;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(
    UART_HandleTypeDef *uart,
    uint8_t *data,
    uint16_t size
)
{
    (void)uart;
    (void)data;
    (void)size;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef *uart,
    const uint8_t *data,
    uint16_t size,
    uint32_t timeout
)
{
    CHECK(uart != NULL);
    CHECK(data != NULL);
    CHECK(timeout == 100U);
    CHECK((log_length + size) < sizeof(log_output));
    memcpy(&log_output[log_length], data, size);
    log_length += size;
    log_output[log_length] = '\0';
    return HAL_OK;
}

bool mpu6050_init(I2C_HandleTypeDef *i2c)
{
    CHECK(i2c != NULL);
    return mpu_init_ok;
}

bool mpu6050_read(mpu6050_data_t *data)
{
    CHECK(data != NULL);
    if (mpu_read_ok)
    {
        *data = next_sample;
    }
    return mpu_read_ok;
}

uint8_t mpu6050_get_address(void)
{
    return mpu_init_ok ? 0x68U : 0U;
}

HAL_StatusTypeDef message_router_publish_local(message_type_t message)
{
    (void)message;
    return HAL_OK;
}

static void process_sample(void)
{
    tick += 50U;
    safety_service_process();
}

static void reset_service(I2C_HandleTypeDef *i2c, UART_HandleTypeDef *uart)
{
    tick = 0U;
    mpu_init_ok = true;
    mpu_read_ok = true;
    next_sample = (mpu6050_data_t){
        .accel_x = 0.2f,
        .accel_y = -0.3f,
        .accel_z = 0.9327379f,
        .gyro_x = 0.2f,
        .gyro_y = -0.1f,
        .gyro_z = 0.05f,
    };
    log_length = 0U;
    log_output[0] = '\0';
    safety_service_init(i2c);
    safety_service_set_log_uart(uart);
}

static void success_and_unstable_timeout(void)
{
    I2C_HandleTypeDef i2c = {1U};
    UART_HandleTypeDef uart = {2U};
    reset_service(&i2c, &uart);
    CHECK(!safety_service_take_calibration_completed());

    CHECK(safety_service_start_calibration());
    CHECK(strcmp(log_output, "CAL_START\r\n") == 0);
    for (uint32_t sample = 0U;
         sample < SAFETY_CALIBRATION_REQUIRED_SAMPLES;
         ++sample)
    {
        process_sample();
    }
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_READY);
    CHECK(status->calibration_valid);
    CHECK(strstr(log_output, "CAL_OK,ax_mg=200,ay_mg=-300") != NULL);
    CHECK(safety_service_take_calibration_completed());
    CHECK(!safety_service_take_calibration_completed());

    log_length = 0U;
    log_output[0] = '\0';
    CHECK(safety_service_start_calibration());
    next_sample.gyro_x = 6.0f;
    for (uint32_t sample = 0U; sample < 200U; ++sample)
    {
        process_sample();
    }
    status = safety_service_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_FAILED_UNSTABLE);
    CHECK(status->calibration_valid); /* 직전 정상 기준은 보존합니다. */
    CHECK(strstr(log_output, "CAL_FAIL,UNSTABLE\r\n") != NULL);
    CHECK(!safety_service_take_calibration_completed());
}

static void sensor_failure_and_reject_are_logged(void)
{
    I2C_HandleTypeDef i2c = {1U};
    UART_HandleTypeDef uart = {2U};
    reset_service(&i2c, &uart);

    CHECK(safety_service_start_calibration());
    mpu_read_ok = false;
    for (uint32_t failure = 0U; failure < 10U; ++failure)
    {
        process_sample();
    }
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_FAILED_SENSOR);
    CHECK(!status->calibration_valid);
    CHECK(strstr(log_output, "CAL_FAIL,SENSOR\r\n") != NULL);
    CHECK(!safety_service_take_calibration_completed());

    log_length = 0U;
    log_output[0] = '\0';
    CHECK(!safety_service_start_calibration());
    CHECK(strcmp(log_output, "CAL_REJECT,SENSOR_OR_FALL_STATE\r\n") == 0);
    CHECK(!safety_service_take_calibration_completed());
}

int main(void)
{
    success_and_unstable_timeout();
    sensor_failure_and_reject_are_logged();
    puts("PASS nonblocking 50ms samples, 2s success, 10s unstable timeout");
    puts("PASS CAL_START/CAL_OK/CAL_FAIL/CAL_REJECT USART2 text logs");
    puts("PASS calibration-completed one-shot only after READY success");
    puts("HAL_MPU6050_AND_UART=MOCK; PHYSICAL_BICYCLE=NOT_TESTED");
    return 0;
}
