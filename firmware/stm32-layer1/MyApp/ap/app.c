#include "app.h"

#include "button.h"
#include "message_router.h"
#include "message_service.h"
#include "safety_service.h"
#include "uart_service.h"
#include "vs1003b.h"
#if NOSTOS_PROTOCOL_V2
#include "message_protocol_service.h"
#include "audio_service.h"
volatile nostos_result_t protocol_debug_boot_status=NOSTOS_NOT_READY;
#endif

#define VS1003B_SCI_CLOCKF_ADDRESS 0x03U
#define VS1003B_SCI_VOL_ADDRESS 0x0BU
#define VS1003B_CLOCKF_3X 0x9800U
#define VS1003B_TEST_VOLUME 0x5050U

static message_type_t last_message = MSG_NONE;

/* 첫 하드웨어 검증 단계에서 디버거 Watch로 확인할 변수입니다. */
volatile vs1003b_status_t vs1003b_debug_status = VS1003B_STATUS_INVALID_ARGUMENT;
volatile uint16_t vs1003b_debug_mode = 0U;
volatile uint16_t vs1003b_debug_clockf = 0U;
volatile uint16_t vs1003b_debug_volume = 0U;
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
volatile bool safety_debug_distance_valid = false;
volatile float safety_debug_distance_cm = 0.0f;
volatile safety_event_t safety_debug_event = SAFETY_EVENT_NONE;
volatile fall_state_t safety_debug_fall_state = FALL_STATE_IDLE;
volatile uint32_t safety_debug_countdown_seconds = 0U;

void app_init(
    SPI_HandleTypeDef *vs1003b_spi,
    UART_HandleTypeDef *message_uart,
    I2C_HandleTypeDef *sensor_i2c
)
{
    button_init();
    uart_debug_status = uart_service_init(message_uart);

    uint16_t mode = 0U;
    vs1003b_debug_status = vs1003b_init(vs1003b_spi, &mode);
    vs1003b_debug_mode = mode;

    if (vs1003b_debug_status == VS1003B_STATUS_OK)
    {
        vs1003b_debug_status = vs1003b_write_register(
            VS1003B_SCI_CLOCKF_ADDRESS,
            VS1003B_CLOCKF_3X
        );
    }

    if (vs1003b_debug_status == VS1003B_STATUS_OK)
    {
        uint16_t clockf = 0U;
        vs1003b_debug_status = vs1003b_read_register(
            VS1003B_SCI_CLOCKF_ADDRESS,
            &clockf
        );
        vs1003b_debug_clockf = clockf;

        if ((vs1003b_debug_status == VS1003B_STATUS_OK) &&
            (clockf != VS1003B_CLOCKF_3X))
        {
            vs1003b_debug_status = VS1003B_STATUS_REGISTER_MISMATCH;
        }
    }

    if (vs1003b_debug_status == VS1003B_STATUS_OK)
    {
        vs1003b_debug_status = vs1003b_write_register(
            VS1003B_SCI_VOL_ADDRESS,
            VS1003B_TEST_VOLUME
        );
    }

    if (vs1003b_debug_status == VS1003B_STATUS_OK)
    {
        uint16_t volume = 0U;
        vs1003b_debug_status = vs1003b_read_register(
            VS1003B_SCI_VOL_ADDRESS,
            &volume
        );
        vs1003b_debug_volume = volume;

        if ((vs1003b_debug_status == VS1003B_STATUS_OK) &&
            (volume != VS1003B_TEST_VOLUME))
        {
            vs1003b_debug_status = VS1003B_STATUS_REGISTER_MISMATCH;
        }
    }

#if NOSTOS_PROTOCOL_V2
    /* No raw-v1 fallback when trusted session restore is not configured. */
    protocol_debug_boot_status=message_protocol_service_boot(message_uart,vs1003b_debug_status);
#else
    message_service_init(vs1003b_debug_status);
#endif
    message_router_init();
    safety_service_init(sensor_i2c);
}

void app_process(void)
{
    message_type_t message = button_get_message();

    if (message != MSG_NONE)
    {
        last_message = message;
        uart_debug_status = message_router_publish_local(message);
    }

    message_type_t received_message = MSG_NONE;
    if (uart_service_get_message(&received_message))
    {
        uart_debug_last_received = received_message;
        last_message = received_message;
        message_router_deliver_remote(received_message);
    }

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
#if NOSTOS_PROTOCOL_V2
    message_protocol_service_process();
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

    uart_debug_status = uart_service_get_status();
    uart_debug_tx_count = uart_service_get_tx_count();
    uart_debug_rx_count = uart_service_get_rx_count();
    uart_debug_invalid_count = uart_service_get_invalid_count();
    uart_debug_dropped_count = uart_service_get_dropped_count();
    message_router_debug_local_count = message_router_get_local_count();
    message_router_debug_remote_count = message_router_get_remote_count();

    const safety_service_status_t *safety_status = safety_service_get_status();
    safety_debug_mpu_ready = safety_status->mpu_ready;
    safety_debug_mpu_data_valid = safety_status->mpu_data_valid;
    safety_debug_mpu_address = safety_status->mpu_address;
    safety_debug_mpu_failure_count = safety_status->mpu_failure_count;
    safety_debug_distance_valid = safety_status->distance_valid;
    safety_debug_distance_cm = safety_status->distance_cm;
    safety_debug_event = safety_status->event;
    safety_debug_fall_state = safety_status->fall_state;
    safety_debug_countdown_seconds = safety_status->countdown_remaining_seconds;
}

message_type_t app_get_last_message(void)
{
    return last_message;
}
