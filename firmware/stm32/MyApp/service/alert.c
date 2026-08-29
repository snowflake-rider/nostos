#include "alert.h"

#include "main.h"
#include "rgb_led.h"

#define REAR_WARNING_BLINK_MS 500U
#define EMERGENCY_BLINK_MS 200U
#define LOCAL_BUTTON_COLOR_MS 2000U

static alert_state_t alert_state = ALERT_STATE_OFF;
static bool led_on = false;
static uint32_t next_toggle_ms = 0U;
static message_type_t local_button_color = MSG_NONE;
static uint32_t local_button_color_until_ms = 0U;
static bool rear_safe_enabled = false;

static void alert_apply_output(void)
{
    if (local_button_color == MSG_SPEED_UP_REQUEST)
    {
        rgb_led_set(false, true, false);
        return;
    }
    if (local_button_color == MSG_SPEED_DOWN_REQUEST)
    {
        rgb_led_set(true, true, false);
        return;
    }
    if (local_button_color == MSG_STOP_REQUEST)
    {
        rgb_led_set(true, false, false);
        return;
    }

    if (!led_on)
    {
        rgb_led_off();
        return;
    }

    switch (alert_state)
    {
        case ALERT_STATE_REAR_SAFE:
            rgb_led_set(false, true, false);
            break;

        case ALERT_STATE_REAR_WARNING:
            rgb_led_set(true, true, false);
            break;

        case ALERT_STATE_EMERGENCY:
            rgb_led_set(true, false, false);
            break;

        case ALERT_STATE_OFF:
        default:
            rgb_led_off();
            break;
    }
}

static void alert_select_state(alert_state_t state)
{
    if (alert_state == state)
    {
        return;
    }

    alert_state = state;
    led_on = ((state == ALERT_STATE_REAR_SAFE) && rear_safe_enabled) ||
        (state == ALERT_STATE_REAR_WARNING) ||
        (state == ALERT_STATE_EMERGENCY);

    if (state == ALERT_STATE_EMERGENCY)
    {
        local_button_color = MSG_NONE;
    }

    if (state == ALERT_STATE_REAR_WARNING)
    {
        next_toggle_ms = HAL_GetTick() + REAR_WARNING_BLINK_MS;
    }
    else if (state == ALERT_STATE_EMERGENCY)
    {
        next_toggle_ms = HAL_GetTick() + EMERGENCY_BLINK_MS;
    }
    else
    {
        next_toggle_ms = 0U;
    }

    alert_apply_output();
}

void alert_init(void)
{
    rgb_led_init();
    alert_state = ALERT_STATE_OFF;
    led_on = false;
    next_toggle_ms = 0U;
    local_button_color = MSG_NONE;
    local_button_color_until_ms = 0U;
    rear_safe_enabled = false;
}

void alert_set_rear_safe_enabled(bool enabled)
{
    if (rear_safe_enabled == enabled)
    {
        return;
    }

    rear_safe_enabled = enabled;
    if (alert_state == ALERT_STATE_REAR_SAFE)
    {
        led_on = enabled;
        alert_apply_output();
    }
}

void alert_show(message_type_t message)
{
    switch (message)
    {
        case MSG_REAR_SAFE:
            alert_select_state(ALERT_STATE_REAR_SAFE);
            break;

        case MSG_REAR_WARNING:
            alert_select_state(ALERT_STATE_REAR_WARNING);
            break;

        case MSG_FALL_DETECTED:
        case MSG_SOS:
            alert_select_state(ALERT_STATE_EMERGENCY);
            break;

        case MSG_NONE:
        case MSG_SPEED_DOWN_REQUEST:
        case MSG_SPEED_UP_REQUEST:
        case MSG_SAFETY_REMINDER:
        case MSG_STOP_REQUEST:
        case MSG_UNKNOWN:
        default:
            break;
    }
}

void alert_show_local_button(message_type_t message)
{
    if (alert_state == ALERT_STATE_EMERGENCY)
    {
        return;
    }

    if ((message != MSG_SPEED_UP_REQUEST) &&
        (message != MSG_SPEED_DOWN_REQUEST) &&
        (message != MSG_STOP_REQUEST))
    {
        return;
    }

    local_button_color = message;
    local_button_color_until_ms = HAL_GetTick() + LOCAL_BUTTON_COLOR_MS;
    alert_apply_output();
}

void alert_process(void)
{
    if (local_button_color != MSG_NONE)
    {
        if ((int32_t)(HAL_GetTick() - local_button_color_until_ms) < 0)
        {
            return;
        }

        local_button_color = MSG_NONE;
        if (alert_state == ALERT_STATE_REAR_WARNING)
        {
            next_toggle_ms = HAL_GetTick() + REAR_WARNING_BLINK_MS;
        }
        alert_apply_output();
    }

    uint32_t blink_period_ms = 0U;

    if (alert_state == ALERT_STATE_REAR_WARNING)
    {
        blink_period_ms = REAR_WARNING_BLINK_MS;
    }
    else if (alert_state == ALERT_STATE_EMERGENCY)
    {
        blink_period_ms = EMERGENCY_BLINK_MS;
    }

    if ((blink_period_ms != 0U) &&
        ((int32_t)(HAL_GetTick() - next_toggle_ms) >= 0))
    {
        led_on = !led_on;
        next_toggle_ms = HAL_GetTick() + blink_period_ms;
        alert_apply_output();
    }
}

alert_state_t alert_get_state(void)
{
    return alert_state;
}

bool alert_is_led_on(void)
{
    return led_on;
}
