#include "shared_state.h"

#include <stdio.h>

/* 실제 화면 대신 터미널에 B의 온도를 표시하는 학습용 예제입니다. */
static bool print_temperature(const shared_state_t *state, uint32_t now_ms)
{
    shared_sensor_value_t reading;
    shared_value_status_t status = shared_state_get(
        state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C, now_ms, &reading);

    switch (status)
    {
        case SHARED_VALUE_MISSING:
            puts("B 온도: -- (미수신)");
            return true;
        case SHARED_VALUE_FRESH:
            printf("B 온도: %.1f C (정상)\n", (double)reading.value);
            return true;
        case SHARED_VALUE_STALE:
            printf("B 온도: %.1f C (오래됨)\n", (double)reading.value);
            return true;
        default:
            return false;
    }
}

int main(void)
{
    shared_state_t state;
    if (!shared_state_init(&state, 5000U) || !print_temperature(&state, 0U))
    {
        return 1;
    }

    /* 가짜 입력: 1000ms 시점에 B의 온도 28도를 받았다고 가정합니다. */
    if (!shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                             28.0f, 1000U) ||
        !print_temperature(&state, 1000U) || !print_temperature(&state, 6000U))
    {
        return 1;
    }

    /* 실제 0도도 유효한 값입니다. 미수신과 다르게 표시합니다. */
    if (!shared_state_update(&state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
                             0.0f, 6100U) || !print_temperature(&state, 6100U))
    {
        return 1;
    }
    return 0;
}
