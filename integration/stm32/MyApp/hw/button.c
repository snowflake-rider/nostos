#include "button.h"

#include "main.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BUTTON_DEBOUNCE_MS 30U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    message_type_t message;
    bool last_raw_pressed;
    bool stable_pressed;
    uint32_t raw_changed_at_ms;
} button_state_t;

static button_state_t buttons[] = {
    {BTN1_GPIO_Port, BTN1_Pin, MSG_SPEED_DOWN_REQUEST, false, false, 0U},
    {BTN2_GPIO_Port, BTN2_Pin, MSG_SPEED_UP_REQUEST, false, false, 0U},
    {BTN3_GPIO_Port, BTN3_Pin, MSG_SAFETY_REMINDER, false, false, 0U},
    {BTN4_GPIO_Port, BTN4_Pin, MSG_STOP_REQUEST, false, false, 0U},
};

static bool button_read_pressed(const button_state_t *button)
{
    /* 내부 Pull-up 회로이므로 LOW(GPIO_PIN_RESET)가 눌림입니다. */
    return HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET;
}

void button_init(void)
{
    uint32_t now_ms = HAL_GetTick();

    for (size_t i = 0U; i < (sizeof(buttons) / sizeof(buttons[0])); ++i)
    {
        bool pressed = button_read_pressed(&buttons[i]);

        buttons[i].last_raw_pressed = pressed;
        buttons[i].stable_pressed = pressed;
        buttons[i].raw_changed_at_ms = now_ms;
    }
}

message_type_t button_get_message(void)
{
    uint32_t now_ms = HAL_GetTick();
    message_type_t event = MSG_NONE;

    for (size_t i = 0U; i < (sizeof(buttons) / sizeof(buttons[0])); ++i)
    {
        bool raw_pressed = button_read_pressed(&buttons[i]);

        if (raw_pressed != buttons[i].last_raw_pressed)
        {
            buttons[i].last_raw_pressed = raw_pressed;
            buttons[i].raw_changed_at_ms = now_ms;
        }

        if ((raw_pressed != buttons[i].stable_pressed) &&
            ((uint32_t)(now_ms - buttons[i].raw_changed_at_ms) >= BUTTON_DEBOUNCE_MS))
        {
            buttons[i].stable_pressed = raw_pressed;

            /* 눌림 전이만 이벤트로 만들고 버튼을 놓는 동작은 무시합니다. */
            if (raw_pressed && (event == MSG_NONE))
            {
                event = buttons[i].message;
            }
        }
    }

    return event;
}
