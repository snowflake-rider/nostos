#include "safety_service.h"

#include "message_router.h"
#include "mpu6050.h"
#include "sensor_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static bool mpu_init_ok;
static uint32_t mpu_init_calls;
static bool mpu_read_ok;
static mpu6050_data_t next_sample;
static char log_output[512];
static size_t log_length;
static message_type_t last_published_message;
static uint32_t published_message_count;

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
    ++mpu_init_calls;
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
    last_published_message = message;
    ++published_message_count;
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
    mpu_init_calls = 0U;
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
    last_published_message = MSG_NONE;
    published_message_count = 0U;
    sensor_store_init();
    safety_service_init(i2c);
    safety_service_set_log_uart(uart);
}

static void start_automatic_calibration(void)
{
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_INIT);
    CHECK(status->cal_session_id == 1U);
    CHECK(safety_service_buttons_blocked());
    CHECK(mpu_init_calls == 0U);

    tick = SAFETY_CAL_INIT_DISPLAY_MS - 1U;
    safety_service_process();
    CHECK(mpu_init_calls == 0U);

    tick = SAFETY_CAL_INIT_DISPLAY_MS;
    safety_service_process();
    status = safety_service_get_status();
    CHECK(mpu_init_calls == 1U);
    CHECK(status->mpu_ready);
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_RUNNING);
    CHECK(status->calibration_state == SAFETY_CALIBRATION_COLLECTING);
    CHECK(strcmp(log_output, "CAL_START\r\n") == 0);
}

static void automatic_success_reaches_dashboard(void)
{
    I2C_HandleTypeDef i2c = {1U};
    UART_HandleTypeDef uart = {2U};
    reset_service(&i2c, &uart);
    CHECK(!safety_service_take_calibration_completed());
    start_automatic_calibration();

    for (uint32_t sample = 0U;
         sample < SAFETY_CALIBRATION_REQUIRED_SAMPLES;
         ++sample)
    {
        process_sample();
        if (sample == 19U)
        {
            CHECK(safety_service_get_status()->
                calibration_progress_per_mille == 500U);
        }
    }
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_READY);
    CHECK(status->calibration_valid);
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_SUCCESS);
    CHECK(status->calibration_progress_per_mille == 1000U);
    CHECK(safety_service_buttons_blocked());
    CHECK(strstr(log_output, "CAL_OK,ax_mg=200,ay_mg=-300") != NULL);
    CHECK(safety_service_take_calibration_completed());
    CHECK(!safety_service_take_calibration_completed());

    tick += SAFETY_CAL_SUCCESS_DISPLAY_MS - 1U;
    safety_service_process();
    CHECK(safety_service_get_status()->cal_session_state ==
        SAFETY_CAL_SESSION_SUCCESS);
    ++tick;
    safety_service_process();
    CHECK(safety_service_get_status()->cal_session_state ==
        SAFETY_CAL_SESSION_READY);
    CHECK(!safety_service_buttons_blocked());
}

static void unstable_failure_and_button_hold_retry(void)
{
    I2C_HandleTypeDef i2c = {1U};
    UART_HandleTypeDef uart = {2U};
    reset_service(&i2c, &uart);
    start_automatic_calibration();

    next_sample.gyro_x = 6.0f;
    for (uint32_t sample = 0U;
         sample < (SAFETY_CAL_RUNNING_TIMEOUT_MS / 50U);
         ++sample)
    {
        process_sample();
    }
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_FAILED_UNSTABLE);
    CHECK(!status->calibration_valid);
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_REQUIRED);
    CHECK(strstr(log_output, "CAL_FAIL,UNSTABLE\r\n") != NULL);
    CHECK(!safety_service_take_calibration_completed());

    safety_service_set_recalibration_button(true);
    tick += 1800U;
    safety_service_process();
    status = safety_service_get_status();
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_REQUIRED);
    CHECK(status->recalibration_hold_ms == 1800U);
    CHECK(status->calibration_progress_per_mille == 600U);

    safety_service_set_recalibration_button(false);
    CHECK(safety_service_get_status()->recalibration_hold_ms == 0U);
    safety_service_set_recalibration_button(true);
    tick += SAFETY_RECALIBRATION_HOLD_MS - 1U;
    safety_service_process();
    CHECK(safety_service_get_status()->cal_session_state ==
        SAFETY_CAL_SESSION_REQUIRED);
    ++tick;
    safety_service_process();
    status = safety_service_get_status();
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_INIT);
    CHECK(status->cal_session_id == 2U);
    CHECK(status->recalibration_hold_ms == 0U);
}

static void sensor_init_and_read_failure_require_retry(void)
{
    I2C_HandleTypeDef i2c = {1U};
    UART_HandleTypeDef uart = {2U};
    reset_service(&i2c, &uart);
    mpu_init_ok = false;

    tick = SAFETY_CAL_INIT_DISPLAY_MS;
    safety_service_process();
    tick = SAFETY_CAL_INIT_DISPLAY_MS + 1000U;
    safety_service_process();
    tick = SAFETY_CAL_INIT_DISPLAY_MS + 2000U;
    safety_service_process();
    tick = SAFETY_CAL_INIT_TIMEOUT_MS;
    safety_service_process();
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(mpu_init_calls == 3U);
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_REQUIRED);
    CHECK(status->calibration_state == SAFETY_CALIBRATION_FAILED_SENSOR);
    CHECK(strstr(log_output, "CAL_FAIL,SENSOR\r\n") != NULL);

    reset_service(&i2c, &uart);
    start_automatic_calibration();

    mpu_read_ok = false;
    for (uint32_t failure = 0U; failure < 10U; ++failure)
    {
        process_sample();
    }
    status = safety_service_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_FAILED_SENSOR);
    CHECK(!status->calibration_valid);
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_REQUIRED);
    CHECK(strstr(log_output, "CAL_FAIL,SENSOR\r\n") != NULL);
    CHECK(!safety_service_take_calibration_completed());

    log_length = 0U;
    log_output[0] = '\0';
    CHECK(!safety_service_start_calibration());
    CHECK(strcmp(log_output, "CAL_REJECT,SENSOR_OR_FALL_STATE\r\n") == 0);
    CHECK(!safety_service_take_calibration_completed());
}

static void sensor_loss_does_not_cancel_fall_countdown(void)
{
    I2C_HandleTypeDef i2c = {1U};
    UART_HandleTypeDef uart = {2U};
    reset_service(&i2c, &uart);
    start_automatic_calibration();
    for (uint32_t sample = 0U;
         sample < SAFETY_CALIBRATION_REQUIRED_SAMPLES;
         ++sample)
    {
        process_sample();
    }
    tick += SAFETY_CAL_SUCCESS_DISPLAY_MS;
    safety_service_process();
    CHECK(safety_service_get_status()->cal_session_state ==
        SAFETY_CAL_SESSION_READY);

    next_sample.accel_x = 0.0f;
    next_sample.accel_y = 0.0f;
    next_sample.accel_z = 2.0f;
    process_sample();
    CHECK(safety_service_get_status()->fall_state ==
        FALL_STATE_IMPACT_DETECTED);

    next_sample.accel_x = 1.0f;
    next_sample.accel_y = 0.0f;
    next_sample.accel_z = 0.0f;
    next_sample.gyro_x = 0.2f;
    next_sample.gyro_y = -0.1f;
    next_sample.gyro_z = 0.05f;
    for (uint32_t sample = 0U; sample < 20U; ++sample)
    {
        process_sample();
    }
    CHECK(safety_service_get_status()->fall_state == FALL_STATE_COUNTDOWN);

    mpu_read_ok = false;
    for (uint32_t failure = 0U; failure < 10U; ++failure)
    {
        process_sample();
    }
    const safety_service_status_t *status = safety_service_get_status();
    CHECK(!status->mpu_ready);
    CHECK(status->cal_session_state == SAFETY_CAL_SESSION_READY);
    CHECK(status->fall_state == FALL_STATE_COUNTDOWN);

    for (uint32_t sample = 0U; sample < 190U; ++sample)
    {
        process_sample();
    }
    status = safety_service_get_status();
    CHECK(status->fall_state == FALL_STATE_DETECTED);
    CHECK(status->event == SAFETY_EVENT_FALL_DETECTED);
    CHECK(last_published_message == MSG_FALL_DETECTED);
    CHECK(published_message_count == 1U);
}

int main(void)
{
    automatic_success_reaches_dashboard();
    unstable_failure_and_button_hold_retry();
    sensor_init_and_read_failure_require_retry();
    sensor_loss_does_not_cancel_fall_countdown();
    puts("PASS boot CAL INIT, 2s auto calibration, CAL OK, dashboard READY");
    puts("PASS CAL REQUIRED and BTN1 release-reset/3s hold retry session");
    puts("PASS init/read failure and nonblocking calibration timeouts");
    puts("PASS FALL countdown survives MPU loss and publishes FALL_DETECTED");
    puts("HAL_MPU6050_AND_UART=MOCK; PHYSICAL_BICYCLE=NOT_TESTED");
    return 0;
}
