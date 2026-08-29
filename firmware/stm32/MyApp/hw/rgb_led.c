#include "rgb_led.h"

#include "main.h"

static GPIO_PinState rgb_led_to_pin_state(bool on)
{
    return on ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void rgb_led_init(void)
{
    rgb_led_off();
}

void rgb_led_set(bool red, bool green, bool blue)
{
    HAL_GPIO_WritePin(
        RGB_R_GPIO_Port,
        RGB_R_Pin,
        rgb_led_to_pin_state(red)
    );

    HAL_GPIO_WritePin(
        RGB_G_GPIO_Port,
        RGB_G_Pin,
        rgb_led_to_pin_state(green)
    );

    HAL_GPIO_WritePin(
        RGB_B_GPIO_Port,
        RGB_B_Pin,
        rgb_led_to_pin_state(blue)
    );
}

void rgb_led_off(void)
{
    rgb_led_set(false, false, false);
}