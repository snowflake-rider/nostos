#include "app.h"
#include "app_runtime.h"

#include "app_config.h"
#include "audio_service.h"
#include "alert.h"
#include "button.h"
#include "display_service.h"
#include "environment_service.h"
#include "message_router.h"
#include "message_service.h"
#include "safety_service.h"
#include "sensor_store.h"
#include "sensor_sync_service.h"
#include "sensor_view_service.h"
#include "uart_service.h"
#include "vs1003b.h"
#if NOSTOS_PROTOCOL_V2
#include "message_protocol_service.h"
volatile nostos_result_t protocol_debug_boot_status=NOSTOS_NOT_READY;
#endif

#define VS1003B_SCI_CLOCKF_ADDRESS 0x03U

static message_type_t last_message = MSG_NONE;

/* 첫 하드웨어 검증 단계에서 디버거 Watch로 확인할 변수입니다. */
volatile vs1003b_status_t vs1003b_debug_status = VS1003B_STATUS_INVALID_ARGUMENT;
volatile uint16_t vs1003b_debug_mode = 0U;
volatile uint16_t vs1003b_debug_clockf = 0U;
volatile bool vs1003b_debug_audio_playing = false;
volatile uint32_t vs1003b_debug_audio_position = 0U;
volatile bool buzzer_debug_active = false;
volatile buzzer_pattern_t buzzer_debug_pattern = BUZZER_PATTERN_NONE;
volatile alert_state_t alert_debug_state = ALERT_STATE_OFF;
volatile bool alert_debug_led_on = false;
volatile HAL_StatusTypeDef uart_debug_status = HAL_ERROR;
volatile uint32_t uart_debug_tx_count = 0U;
volatile uint32_t uart_debug_rx_count = 0U;
volatile uint32_t uart_debug_invalid_count = 0U;
volatile uint32_t uart_debug_dropped_count = 0U;
volatile message_type_t uart_debug_last_received = MSG_NONE;
volatile message_type_t message_debug_inject = MSG_NONE;
volatile uint32_t message_debug_inject_count = 0U;
volatile uint32_t message_router_debug_local_count = 0U;
volatile uint32_t message_router_debug_remote_count = 0U;
volatile bool safety_debug_mpu_ready = false;
volatile bool safety_debug_mpu_data_valid = false;
volatile uint8_t safety_debug_mpu_address = 0U;
volatile uint32_t safety_debug_mpu_failure_count = 0U;
volatile safety_event_t safety_debug_event = SAFETY_EVENT_NONE;
volatile fall_state_t safety_debug_fall_state = FALL_STATE_IDLE;
volatile uint32_t safety_debug_countdown_seconds = 0U;
volatile safety_calibration_state_t safety_debug_calibration_state =
    SAFETY_CALIBRATION_UNCALIBRATED;
volatile bool safety_debug_calibration_valid = false;
volatile uint32_t safety_debug_calibration_samples = 0U;
volatile bool safety_debug_calibration_request_accepted = false;
volatile bool display_debug_ready = false;
volatile bool environment_debug_valid = false;
volatile uint32_t environment_debug_failure_count = 0U;

void app_init(
    SPI_HandleTypeDef *vs1003b_spi,
    UART_HandleTypeDef *message_uart,
    I2C_HandleTypeDef *sensor_i2c
)
{
    button_init();
    sensor_store_init();
    sensor_sync_service_init();
    sensor_view_service_init();
    uart_debug_status = uart_service_init(message_uart);

    uint16_t mode = 0U;
    vs1003b_debug_status = vs1003b_init(vs1003b_spi, &mode);
    vs1003b_debug_mode = mode;

    if (vs1003b_debug_status == VS1003B_STATUS_OK)
    {
        uint16_t clockf = 0U;
        vs1003b_debug_status = vs1003b_read_register(
            VS1003B_SCI_CLOCKF_ADDRESS,
            &clockf
        );
        vs1003b_debug_clockf = clockf;
    }

#if NOSTOS_PROTOCOL_V2
    /* No raw-v1 fallback when trusted session restore is not configured. */
    protocol_debug_boot_status=message_protocol_service_boot(message_uart,vs1003b_debug_status);
#else
    message_service_init(vs1003b_debug_status);
#endif
    message_router_init();
    safety_service_init(sensor_i2c);
    environment_service_init();
    display_service_init(sensor_i2c);
}

message_type_t app_runtime_poll_button(bool *reset_requested)
{
    message_type_t message = button_get_message();
    bool reset = button_take_output_reset_request();
    if (reset_requested != NULL)
    {
        *reset_requested = reset;
    }
    return reset ? MSG_NONE : message;
}

bool app_runtime_poll_remote(message_type_t *message)
{
    return uart_service_get_message(message);
}

void app_runtime_reset(void)
{
    last_message = MSG_NONE;
    message_debug_inject = MSG_NONE;
    uart_service_clear_pending();
    (void)safety_service_take_calibration_completed();
#if NOSTOS_PROTOCOL_V2
    alert_reset();
    buzzer_stop();
    vs1003b_debug_status = audio_service_stop();
#else
    message_service_reset_outputs();
    vs1003b_debug_status = message_service_get_status()->audio_status;
#endif
}

void app_runtime_dispatch_local(message_type_t message)
{
    if ((message != MSG_NONE) && (message != MSG_UNKNOWN))
    {
        last_message = message;
        uart_debug_status = message_router_publish_local(message);
    }
}

void app_runtime_dispatch_remote(message_type_t message)
{
    if ((message != MSG_NONE) && (message != MSG_UNKNOWN))
    {
        last_message = message;
        uart_debug_last_received = message;
        message_router_deliver_remote(message);
    }
}

void app_runtime_process_services(void)
{
    /* 팀 장치 연결 전 외부 메시지 동작을 확인하기 위한 디버거 주입 지점입니다. */
    message_type_t injected_message = message_debug_inject;
    if (injected_message != MSG_NONE)
    {
        message_debug_inject = MSG_NONE;
        ++message_debug_inject_count;
        last_message = injected_message;
        message_router_deliver_remote(injected_message);
    }

    safety_service_process();
    environment_service_process();
    const safety_service_status_t *safety_status = safety_service_get_status();
    if (!audio_service_is_playing() &&
        safety_service_take_calibration_completed())
    {
        vs1003b_debug_status = audio_service_play_calibration_completed();
    }
#if NOSTOS_PROTOCOL_V2
    message_protocol_service_process();
    sensor_sync_service_process();
    vs1003b_debug_audio_playing=audio_service_is_playing();
    vs1003b_debug_audio_position=audio_service_position();
    buzzer_debug_active=buzzer_is_active();
    buzzer_debug_pattern=buzzer_get_pattern();
    alert_debug_state=alert_get_state();
    alert_debug_led_on=alert_is_led_on();
#else
    message_service_process();

    const message_service_status_t *status = message_service_get_status();
    vs1003b_debug_status = status->audio_status;
    vs1003b_debug_audio_playing = status->audio_playing;
    vs1003b_debug_audio_position = status->audio_position;
    buzzer_debug_active = status->buzzer_active;
    buzzer_debug_pattern = status->buzzer_pattern;
    alert_debug_state = status->alert_state;
    alert_debug_led_on = status->alert_led_on;
#endif
    display_service_process();

    uart_debug_status = uart_service_get_status();
    uart_debug_tx_count = uart_service_get_tx_count();
    uart_debug_rx_count = uart_service_get_rx_count();
    uart_debug_invalid_count = uart_service_get_invalid_count();
    uart_debug_dropped_count = uart_service_get_dropped_count();
    message_router_debug_local_count = message_router_get_local_count();
    message_router_debug_remote_count = message_router_get_remote_count();

    safety_debug_mpu_ready = safety_status->mpu_ready;
    safety_debug_mpu_data_valid = safety_status->mpu_data_valid;
    safety_debug_mpu_address = safety_status->mpu_address;
    safety_debug_mpu_failure_count = safety_status->mpu_failure_count;
    safety_debug_event = safety_status->event;
    safety_debug_fall_state = safety_status->fall_state;
    safety_debug_countdown_seconds = safety_status->countdown_remaining_seconds;
    safety_debug_calibration_state = safety_status->calibration_state;
    safety_debug_calibration_valid = safety_status->calibration_valid;
    safety_debug_calibration_samples = safety_status->calibration_sample_count;
    display_debug_ready = display_service_is_ready();
    environment_debug_valid = environment_service_data_valid();
    environment_debug_failure_count = environment_service_failure_count();
}

void app_process(void)
{
    bool reset_requested = false;
    message_type_t message = app_runtime_poll_button(&reset_requested);
    if (reset_requested)
    {
        app_runtime_reset();
        return;
    }

    app_runtime_dispatch_local(message);

    message_type_t received_message = MSG_NONE;
    if (app_runtime_poll_remote(&received_message))
    {
        app_runtime_dispatch_remote(received_message);
    }

    app_runtime_process_services();
}

message_type_t app_get_last_message(void)
{
    return last_message;
}
