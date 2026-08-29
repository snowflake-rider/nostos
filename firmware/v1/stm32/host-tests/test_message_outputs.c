#include "main.h"
#include "message_service.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static unsigned audio_play_count;
static message_type_t last_audio_message;

uint32_t HAL_GetTick(void)
{
    return tick;
}

void HAL_GPIO_WritePin(
    GPIO_TypeDef *port,
    uint16_t pin,
    GPIO_PinState state
)
{
    if (state == GPIO_PIN_SET)
    {
        port->output |= pin;
    }
    else
    {
        port->output &= (uint16_t)~pin;
    }
}

vs1003b_status_t audio_service_play(message_type_t message)
{
    ++audio_play_count;
    last_audio_message = message;
    return VS1003B_STATUS_OK;
}

vs1003b_status_t audio_service_process(void)
{
    return VS1003B_STATUS_OK;
}

bool audio_service_is_playing(void)
{
    return false;
}

uint32_t audio_service_position(void)
{
    return 0U;
}

static bool red_on(void)
{
    return (host_gpio_a.output & GPIO_PIN_4) != 0U;
}

static bool green_on(void)
{
    return (host_gpio_b.output & GPIO_PIN_0) != 0U;
}

static bool blue_on(void)
{
    return (host_gpio_c.output & GPIO_PIN_1) != 0U;
}

static bool buzzer_pin_on(void)
{
    return (host_gpio_b.output & GPIO_PIN_4) != 0U;
}

static void reset_outputs(void)
{
    host_gpio_a = (GPIO_TypeDef){0};
    host_gpio_b = (GPIO_TypeDef){0};
    host_gpio_c = (GPIO_TypeDef){0};
    tick = 100U;
    audio_play_count = 0U;
    last_audio_message = MSG_NONE;
    message_service_init(VS1003B_STATUS_OK);
}

static void check_local_button(
    message_type_t message,
    bool expected_red,
    bool expected_green
)
{
    reset_outputs();
    message_service_handle_local(message);

    CHECK(red_on() == expected_red);
    CHECK(green_on() == expected_green);
    CHECK(!blue_on());
    CHECK(!buzzer_pin_on());
    CHECK(buzzer_get_pattern() == BUZZER_PATTERN_NONE);
    CHECK(audio_play_count == 1U && last_audio_message == message);

    tick += 1999U;
    message_service_process();
    CHECK(red_on() == expected_red);
    CHECK(green_on() == expected_green);

    ++tick;
    message_service_process();
    CHECK(!red_on() && !green_on() && !blue_on());
}

static void local_buttons_show_color_and_audio(void)
{
    check_local_button(MSG_SPEED_UP_REQUEST, false, true);
    check_local_button(MSG_SPEED_DOWN_REQUEST, true, true);
    check_local_button(MSG_STOP_REQUEST, true, false);
}

static void remote_buttons_are_audio_only(void)
{
    const message_type_t messages[] = {
        MSG_SPEED_UP_REQUEST,
        MSG_SPEED_DOWN_REQUEST,
        MSG_STOP_REQUEST,
    };

    for (size_t index = 0U; index < 3U; ++index)
    {
        reset_outputs();
        message_service_handle(messages[index]);
        CHECK(audio_play_count == 1U);
        CHECK(last_audio_message == messages[index]);
        CHECK(!red_on() && !green_on() && !blue_on());
        CHECK(!buzzer_pin_on());
        CHECK(buzzer_get_pattern() == BUZZER_PATTERN_NONE);
    }
}

static void rear_safe_and_button_timeout_follow_calibration(void)
{
    reset_outputs();
    message_service_handle(MSG_REAR_SAFE);

    CHECK(alert_get_state() == ALERT_STATE_REAR_SAFE);
    CHECK(!alert_is_led_on());
    CHECK(!red_on() && !green_on() && !blue_on());

    alert_set_rear_safe_enabled(true);
    CHECK(alert_get_state() == ALERT_STATE_REAR_SAFE);
    CHECK(alert_is_led_on());
    CHECK(!red_on() && green_on() && !blue_on());

    message_service_handle_local(MSG_SPEED_DOWN_REQUEST);
    CHECK(red_on() && green_on() && !blue_on());

    tick += 2000U;
    message_service_process();
    CHECK(alert_get_state() == ALERT_STATE_REAR_SAFE);
    CHECK(alert_is_led_on());
    CHECK(!red_on() && green_on() && !blue_on());

    alert_set_rear_safe_enabled(false);
    CHECK(!alert_is_led_on());
    CHECK(!red_on() && !green_on() && !blue_on());
}

static void only_fall_starts_buzzer(void)
{
    reset_outputs();
    message_service_handle(MSG_REAR_WARNING);
    CHECK(audio_play_count == 1U);
    CHECK(buzzer_get_pattern() == BUZZER_PATTERN_NONE);
    CHECK(!buzzer_pin_on());

    reset_outputs();
    message_service_handle(MSG_SOS);
    CHECK(buzzer_get_pattern() == BUZZER_PATTERN_NONE);
    CHECK(!buzzer_pin_on());

    /* SOS가 먼저 와도 뒤의 실제 FALL은 부저를 시작해야 합니다. */
    message_service_handle(MSG_FALL_DETECTED);
    CHECK(buzzer_get_pattern() == BUZZER_PATTERN_EMERGENCY);
    CHECK(buzzer_pin_on());
}

int main(void)
{
    local_buttons_show_color_and_audio();
    remote_buttons_are_audio_only();
    rear_safe_and_button_timeout_follow_calibration();
    only_fall_starts_buzzer();
    puts("PASS local BTN1/2/3 RGB+audio and 2-second color timeout");
    puts("PASS remote BTN1/2/3 audio-only");
    puts("PASS REAR_SAFE green only after calibration and button timeout restores it");
    puts("PASS buzzer starts only for confirmed FALL_DETECTED");
    return 0;
}
