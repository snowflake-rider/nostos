/* 김현수, Redpill Day 25. See README.md for contracts and adaptations. */
#ifndef REDPILL_MOVING_AVERAGE_H
#define REDPILL_MOVING_AVERAGE_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

#define rp25_WINDOW_SIZE 5

typedef struct
{
    float buffer[rp25_WINDOW_SIZE];
    size_t buffer_idx;
    size_t count;
    double window_sum;
} rp25_SlidingWindow;

void rp25_sliding_window_init(rp25_SlidingWindow *sw);

/* First five finite values are warm-up; then reject outside mean +/- 50%.
 * Rejected values leave the window unchanged. Initialize before calling. */
bool rp25_sliding_window_update(rp25_SlidingWindow *sw, float input);

float rp25_moving_average(const rp25_SlidingWindow *sw);

int rp25_demo(void);

#endif
