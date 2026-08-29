#include "button.h"

#include "main.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BUTTON_DEBOUNCE_MS 30U
#define CALIBRATION_SEQUENCE_TIMEOUT_MS 5000U
#define CALIBRATION_SEQUENCE_BUTTON_COUNT 4U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    message_type_t message;
    bool last_raw_pressed;
    bool stable_pressed;
    bool press_armed;
    uint32_t raw_changed_at_ms;
} button_state_t;

static button_state_t buttons[] = {
    {BTN1_GPIO_Port, BTN1_Pin, MSG_SPEED_UP_REQUEST,
     false, false, false, 0U},
    {BTN2_GPIO_Port, BTN2_Pin, MSG_SPEED_DOWN_REQUEST,
     false, false, false, 0U},
    {BTN3_GPIO_Port, BTN3_Pin, MSG_STOP_REQUEST,
     false, false, false, 0U},
    {BTN4_GPIO_Port, BTN4_Pin, MSG_NONE,
     false, false, false, 0U},
    /* D10/PB6 테스트 버튼도 기존 정지 요청 1바이트를 사용합니다. */
    {TEST_BUTTON_GPIO_Port, TEST_BUTTON_Pin, MSG_STOP_REQUEST,
     false, false, false, 0U},
};

static bool calibration_requested = false;
static uint8_t calibration_sequence_step = 0U;
static uint32_t calibration_sequence_started_at_ms = 0U;

static bool button_read_pressed(const button_state_t *button)
{
    /* 내부 Pull-up 회로이므로 LOW(GPIO_PIN_RESET)가 눌림입니다. */
    return HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET;
}

static bool calibration_sequence_timed_out(uint32_t now_ms)
{
    return (calibration_sequence_step != 0U) &&
        ((uint32_t)(now_ms - calibration_sequence_started_at_ms) >=
         CALIBRATION_SEQUENCE_TIMEOUT_MS);
}

static void calibration_sequence_register_press(
    size_t button_index,
    uint32_t now_ms
)
{
    if (calibration_sequence_timed_out(now_ms))
    {
        calibration_sequence_step = 0U;
    }

    if (button_index == 0U)
    {
        calibration_sequence_step = 1U;
        calibration_sequence_started_at_ms = now_ms;
        return;
    }

    if ((button_index < CALIBRATION_SEQUENCE_BUTTON_COUNT) &&
        (calibration_sequence_step == button_index))
    {
        ++calibration_sequence_step;

        if (calibration_sequence_step == CALIBRATION_SEQUENCE_BUTTON_COUNT)
        {
            calibration_requested = true;
            calibration_sequence_step = 0U;
        }
        return;
    }

    /* BTN2~4 또는 테스트 버튼이 순서와 다르면 처음부터 다시 입력합니다. */
    calibration_sequence_step = 0U;
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

    calibration_requested = false;
    calibration_sequence_step = 0U;
    calibration_sequence_started_at_ms = now_ms;
}

message_type_t button_get_message(void)
{
    uint32_t now_ms = HAL_GetTick();
    message_type_t event = MSG_NONE;
    bool sequence_press_seen_this_poll = false;

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
                    if (!sequence_press_seen_this_poll)
                    {
                        calibration_sequence_register_press(i, now_ms);
                        sequence_press_seen_this_poll = true;
                    }
                    else
                    {
                        /* 같은 poll에서 둘 이상 눌리면 순차 입력이 아닙니다. */
                        calibration_sequence_step = 0U;
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

    if (calibration_sequence_timed_out(now_ms))
    {
        calibration_sequence_step = 0U;
    }

    return event;
}

bool button_take_calibration_request(void)
{
    bool requested = calibration_requested;
    calibration_requested = false;
    return requested;
}
