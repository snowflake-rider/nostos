# Day 25 — 김현수 원문

출처: https://app.notion.com/63e2ae70087183cb8cea0159e390ff03

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_SIZE 5
typedef struct
{
    float buffer[WINDOW_SIZE];
    size_t buffer_idx;
    size_t count;
    double window_sum;
} SlidingWindow;

static void sliding_window_init(SlidingWindow *sw);
static bool sliding_window_update(SlidingWindow *sw, float input);
static float moving_average(const SlidingWindow *sw);

int main(void)
{
    const float inputs[] = {
        20.0f, 22.0f, 18.0f, 25.0f, 15.0f,
        20.0f, 21.0f, 19.0f, 20.5f, 20.0f};

    SlidingWindow sw;
    sliding_window_init(&sw);

    const size_t input_count = sizeof(inputs) / sizeof(inputs[0]);

    printf("=== Day 25: Moving Average Filter (Sliding Window) ===\n");
    printf("Window Size: %d\n\n", WINDOW_SIZE);
    printf("Step | Raw Input | Filtered Output\n");
    printf("-----+-----------+----------------\n");

    for (size_t i = 0; i < input_count; i++)
    {
        sliding_window_update(&sw, inputs[i]);
        const float average = moving_average(&sw);
        printf("%4zu | %7.1f   | %10.1f\n",
               i + 1,
               inputs[i],
               average);
    }

    return 0;
}

static void sliding_window_init(SlidingWindow *sw)
{
    if (sw == NULL)
    {
        printf("sw must not be NULL.\n");
        return;
    }
    if (WINDOW_SIZE < 1)
    {
        printf("WINDOW_SIZE must be at least 1.\n");
        return;
    }
    *sw = (SlidingWindow){0};
}

static bool sliding_window_update(SlidingWindow *sw, float input)
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
    if (sw->count == WINDOW_SIZE)
    {
        const float curr_mv_avg = moving_average(sw);
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
    sw->buffer_idx = (sw->buffer_idx + 1) % WINDOW_SIZE;
    if (sw->count < WINDOW_SIZE)
    {
        sw->count++;
    }
    return true;
}

static float moving_average(const SlidingWindow *sw)
{
    if (sw == NULL || sw->count == 0)
    {
        printf("Sliding window has no data.\n");
        return NAN;
    }
    return (float)(sw->window_sum / (double)sw->count);
}


````
