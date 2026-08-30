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
    bool resets_outputs;
    bool last_raw_pressed;
    bool stable_pressed;
    bool press_armed;
    uint32_t raw_changed_at_ms;
} button_state_t;

static button_state_t buttons[] = {
    {BTN1_GPIO_Port, BTN1_Pin, MSG_SPEED_UP_REQUEST,
     false, false, false, false, 0U},
    {BTN2_GPIO_Port, BTN2_Pin, MSG_SPEED_DOWN_REQUEST,
     false, false, false, false, 0U},
    {BTN3_GPIO_Port, BTN3_Pin, MSG_STOP_REQUEST,
     false, false, false, false, 0U},
    {BTN4_GPIO_Port, BTN4_Pin, MSG_NONE,
     true, false, false, false, 0U},
    /* D10/PB6 테스트 버튼도 기존 정지 요청 1바이트를 사용합니다. */
    {TEST_BUTTON_GPIO_Port, TEST_BUTTON_Pin, MSG_STOP_REQUEST,
     false, false, false, false, 0U},
};

static bool output_reset_requested = false;

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
        /* 부팅 중 눌린 버튼은 먼저 놓기 전까지 어떤 동작도 만들지 않습니다. */
        buttons[i].press_armed = !pressed;
        buttons[i].raw_changed_at_ms = now_ms;
    }

    output_reset_requested = false;
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

            if (raw_pressed)
            {
                if (buttons[i].press_armed)
                {
                    if (buttons[i].resets_outputs)
                    {
                        output_reset_requested = true;
                    }

                    if ((buttons[i].message != MSG_NONE) &&
                        (event == MSG_NONE))
                    {
                        event = buttons[i].message;
                    }
                }
            }
            else if (!buttons[i].press_armed)
            {
                buttons[i].press_armed = true;
            }
        }
    }

    return event;
}

bool button_is_pressed(button_id_t id)
{
    switch (id)
    {
        case BUTTON_ID_BTN1:
        case BUTTON_ID_BTN2:
        case BUTTON_ID_BTN3:
        case BUTTON_ID_BTN4:
            return buttons[(size_t)id].stable_pressed;
        default:
            return false;
    }
}

bool button_take_output_reset_request(void)
{
    bool requested = output_reset_requested;
    output_reset_requested = false;
    return requested;
}
