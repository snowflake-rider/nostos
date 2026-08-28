#include "moving_average.h"

#include <math.h>

bool comm_moving_average_init(comm_moving_average_t *window)
{
    if (window == NULL) {
        return false;
    }
    *window = (comm_moving_average_t){0};
    return true;
}

bool comm_moving_average_update(comm_moving_average_t *window, float input)
{
    if (window == NULL || !isfinite(input)) {
        return false;
    }

    /* 가장 오래된 측정값을 교체한다. 평균 대비 ±50% 거절은 하지 않는다. */
    window->buffer[window->buffer_idx] = input;
    window->buffer_idx = (window->buffer_idx + 1U) % COMM_MOVING_AVERAGE_WINDOW_SIZE;
    if (window->count < COMM_MOVING_AVERAGE_WINDOW_SIZE) {
        ++window->count;
    }

    /* 5칸만 다시 합산하여, 이전 윈도의 큰 값에서 생긴 누적 오차를 남기지 않는다. */
    window->window_sum = 0.0;
    for (size_t i = 0; i < window->count; ++i) {
        window->window_sum += (double)window->buffer[i];
    }
    return true;
}

bool comm_moving_average_get(const comm_moving_average_t *window, float *average)
{
    if (window == NULL || average == NULL ||
        window->count != COMM_MOVING_AVERAGE_WINDOW_SIZE) {
        return false;
    }

    *average = (float)(window->window_sum / (double)COMM_MOVING_AVERAGE_WINDOW_SIZE);
    return true;
}
