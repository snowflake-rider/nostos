#include "app_config.h"

#if FEATURE_BUTTON_OUTPUT_TEST

#include "button_output_test.h"

#include "audio_service.h"
#include "button.h"
#include "message_service.h"
#include "rgb_led.h"

#define BUTTON_OUTPUT_RGB_MS 2000U

static button_output_test_status_t test_status;
static uint32_t rgb_off_at_ms;

static bool button_output_test_set_rgb(message_type_t message)
{
    switch (message)
    {
        case MSG_SPEED_DOWN_REQUEST:
            rgb_led_set(false, true, false);
            return true;

        case MSG_SPEED_UP_REQUEST:
            rgb_led_set(true, false, false);
            return true;

        case MSG_STOP_REQUEST:
            rgb_led_set(true, true, true);
            return true;

        case MSG_NONE:
        case MSG_FALL_DETECTED:
        case MSG_UNKNOWN:
        default:
            return false;
    }
}

static void button_output_test_update_status(void)
{
    const message_service_status_t *service = message_service_get_status();
    test_status.audio_status = service->audio_status;
    test_status.audio_playing = audio_service_is_playing();
    test_status.audio_position = audio_service_position();
}

void button_output_test_init(void)
{
    rgb_led_off();
    test_status = (button_output_test_status_t){
        .last_message = MSG_NONE,
        .audio_status = message_service_get_status()->audio_status,
    };
    rgb_off_at_ms = HAL_GetTick();
    button_output_test_update_status();
}

void button_output_test_process(void)
{
    message_type_t message = button_get_message();

    if (button_take_output_reset_request())
    {
        message_service_reset_outputs();
        rgb_led_off();
        test_status.last_message = MSG_NONE;
        test_status.rgb_active = false;
        button_output_test_update_status();
        return;
    }

    if (message != MSG_NONE)
    {
        if (button_output_test_set_rgb(message))
        {
            test_status.rgb_active = true;
            rgb_off_at_ms = HAL_GetTick() + BUTTON_OUTPUT_RGB_MS;
        }

        test_status.last_message = message;
        ++test_status.press_count;
        /* 진단 버튼은 ESP32/UART로 송신하지 않고 이 STM32의 출력만 구동합니다. */
        message_service_handle(message);
    }

    message_service_process();

    if (test_status.rgb_active &&
        ((int32_t)(HAL_GetTick() - rgb_off_at_ms) >= 0))
    {
        rgb_led_off();
        test_status.rgb_active = false;
    }

    button_output_test_update_status();
}

const button_output_test_status_t *button_output_test_get_status(void)
{
    return &test_status;
}

#endif /* FEATURE_BUTTON_OUTPUT_TEST */
