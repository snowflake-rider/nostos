#include "buzzer.h"

#include "main.h"

#include <stddef.h>

typedef struct
{
    bool on;
    uint16_t duration_ms;
} buzzer_step_t;

static const buzzer_step_t rear_warning_steps[] = {
    {true, 100U},
    {false, 100U},
    {true, 100U},
};

static const buzzer_step_t emergency_steps[] = {
    {true, 300U},
    {false, 150U},
    {true, 300U},
    {false, 150U},
    {true, 300U},
    {false, 2000U},
};

static bool active = false;
static buzzer_pattern_t current_pattern = BUZZER_PATTERN_NONE;
static const buzzer_step_t *current_steps = NULL;
static size_t current_step_count = 0U;
static size_t current_step_index = 0U;
static uint32_t next_step_ms = 0U;

static void buzzer_write(bool on)
{
    HAL_GPIO_WritePin(
        BUZZER_GPIO_Port,
        BUZZER_Pin,
        on ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}

void buzzer_init(void)
{
    buzzer_stop();
}

void buzzer_stop(void)
{
    buzzer_write(false);
    active = false;
    current_pattern = BUZZER_PATTERN_NONE;
    current_steps = NULL;
    current_step_count = 0U;
    current_step_index = 0U;
    next_step_ms = 0U;
}

void buzzer_play_pattern(buzzer_pattern_t pattern)
{
    if (pattern == BUZZER_PATTERN_NONE)
    {
        buzzer_stop();
        return;
    }

    if ((pattern == BUZZER_PATTERN_EMERGENCY) &&
        (current_pattern == BUZZER_PATTERN_EMERGENCY))
    {
        return;
    }

    current_pattern = pattern;
    current_step_index = 0U;

    if (pattern == BUZZER_PATTERN_REAR_WARNING)
    {
        current_steps = rear_warning_steps;
        current_step_count = sizeof(rear_warning_steps) /
                             sizeof(rear_warning_steps[0]);
    }
    else
    {
        current_steps = emergency_steps;
        current_step_count = sizeof(emergency_steps) /
                             sizeof(emergency_steps[0]);
    }

    active = current_steps[0].on;
    buzzer_write(active);
    next_step_ms = HAL_GetTick() + current_steps[0].duration_ms;
}

void buzzer_process(void)
{
    if ((current_pattern == BUZZER_PATTERN_NONE) ||
        ((int32_t)(HAL_GetTick() - next_step_ms) < 0))
    {
        return;
    }

    ++current_step_index;

    if (current_step_index >= current_step_count)
    {
        if (current_pattern == BUZZER_PATTERN_EMERGENCY)
        {
            current_step_index = 0U;
        }
        else
        {
            buzzer_stop();
            return;
        }
    }

    active = current_steps[current_step_index].on;
    buzzer_write(active);
    next_step_ms = HAL_GetTick() +
                   current_steps[current_step_index].duration_ms;
}

bool buzzer_is_active(void)
{
    return active;
}

buzzer_pattern_t buzzer_get_pattern(void)
{
    return current_pattern;
}
