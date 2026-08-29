#include "ultrasonic.h"

#include "main.h"

#include <stdint.h>

#define ULTRASONIC_ECHO_START_TIMEOUT_US 15000U
#define ULTRASONIC_ECHO_END_TIMEOUT_US 25000U

static uint32_t ticks_per_microsecond = 1U;

static void ultrasonic_delay_us(uint32_t microseconds)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = microseconds * ticks_per_microsecond;
    while ((uint32_t)(DWT->CYCCNT - start) < ticks)
    {
    }
}

void ultrasonic_init(void)
{
    ticks_per_microsecond = HAL_RCC_GetHCLKFreq() / 1000000U;
    if (ticks_per_microsecond == 0U)
    {
        ticks_per_microsecond = 1U;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_RESET);
}

bool ultrasonic_read(float *distance_cm)
{
    uint32_t wait_start;
    uint32_t echo_start;
    uint32_t timeout_ticks;

    if (distance_cm == NULL)
    {
        return false;
    }

    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_RESET);
    ultrasonic_delay_us(2U);
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_SET);
    ultrasonic_delay_us(10U);
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_RESET);

    timeout_ticks = ticks_per_microsecond * ULTRASONIC_ECHO_START_TIMEOUT_US;
    wait_start = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(HCSR04_ECHO_GPIO_Port, HCSR04_ECHO_Pin) == GPIO_PIN_RESET)
    {
        if ((uint32_t)(DWT->CYCCNT - wait_start) > timeout_ticks)
        {
            *distance_cm = 0.0f;
            return false;
        }
    }

    echo_start = DWT->CYCCNT;
    timeout_ticks = ticks_per_microsecond * ULTRASONIC_ECHO_END_TIMEOUT_US;
    while (HAL_GPIO_ReadPin(HCSR04_ECHO_GPIO_Port, HCSR04_ECHO_Pin) == GPIO_PIN_SET)
    {
        if ((uint32_t)(DWT->CYCCNT - echo_start) > timeout_ticks)
        {
            *distance_cm = 0.0f;
            return false;
        }
    }

    float duration_us =
        (float)(uint32_t)(DWT->CYCCNT - echo_start) /
        (float)ticks_per_microsecond;
    float measured_distance = duration_us / 58.0f;

    if ((measured_distance < 2.0f) || (measured_distance > 350.0f))
    {
        *distance_cm = 0.0f;
        return false;
    }

    *distance_cm = measured_distance;
    return true;
}
