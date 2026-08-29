/* 김현수 제출 코드 기반. 원문: originals/day25.md */
#include "moving_average.h"

#include <stdio.h>
#include <stdbool.h>
#include <math.h>




void rp25_sliding_window_init(rp25_SlidingWindow *sw);
bool rp25_sliding_window_update(rp25_SlidingWindow *sw, float input);
float rp25_moving_average(const rp25_SlidingWindow *sw);

int rp25_demo(void)
{
    const float inputs[] = {
        20.0f, 22.0f, 18.0f, 25.0f, 15.0f,
        20.0f, 21.0f, 19.0f, 20.5f, 20.0f};

    rp25_SlidingWindow sw;
    rp25_sliding_window_init(&sw);

    const size_t input_count = sizeof(inputs) / sizeof(inputs[0]);

    printf("=== Day 25: Moving Average Filter (Sliding Window) ===\n");
    printf("Window Size: %d\n\n", rp25_WINDOW_SIZE);
    printf("Step | Raw Input | Filtered Output\n");
    printf("-----+-----------+----------------\n");

    for (size_t i = 0; i < input_count; i++)
    {
        rp25_sliding_window_update(&sw, inputs[i]);
        const float average = rp25_moving_average(&sw);
        printf("%4zu | %7.1f   | %10.1f\n",
               i + 1,
               inputs[i],
               average);
    }

    return 0;
}

void rp25_sliding_window_init(rp25_SlidingWindow *sw)
{
    if (sw == NULL)
    {
        printf("sw must not be NULL.\n");
        return;
    }
    if (rp25_WINDOW_SIZE < 1)
    {
        printf("WINDOW_SIZE must be at least 1.\n");
        return;
    }
    *sw = (rp25_SlidingWindow){0};
}

bool rp25_sliding_window_update(rp25_SlidingWindow *sw, float input)
{
    if (sw == NULL)
    {
        printf("The current sliding window is NULL.\n");
        return false;
    }
    if (!isfinite(input))
    {
        return false;
    }
    if (sw->count == rp25_WINDOW_SIZE)
    {
        const float curr_mv_avg = rp25_moving_average(sw);
        if (!isfinite(curr_mv_avg))
        {
            printf("Invalid Sliding Window.\n");
            return false;
        }
        const float avg_magnitude = curr_mv_avg < 0.0f
                                        ? -curr_mv_avg
                                        : curr_mv_avg;
        const float tolerance = avg_magnitude * 0.5f;
        const float lower_bound = curr_mv_avg - tolerance;
        const float upper_bound = curr_mv_avg + tolerance;
        if (input < lower_bound || upper_bound < input)
        {
            return false;
        }
    }
    //
    // 1. update window_sum
    sw->window_sum += (double)input -
                      (double)sw->buffer[sw->buffer_idx];
    // 2. replace the old buffer item with new input
    sw->buffer[sw->buffer_idx] = input;
    // 3. update buffer idx
    sw->buffer_idx = (sw->buffer_idx + 1) % rp25_WINDOW_SIZE;
    if (sw->count < rp25_WINDOW_SIZE)
    {
        sw->count++;
    }
    return true;
}

float rp25_moving_average(const rp25_SlidingWindow *sw)
{
    if (sw == NULL || sw->count == 0)
    {
        printf("Sliding window has no data.\n");
        return NAN;
    }
    return (float)(sw->window_sum / (double)sw->count);
}
