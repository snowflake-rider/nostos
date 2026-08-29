#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "moving_average.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_waits_for_five_then_slides(void)
{
    comm_moving_average_t window;
    const float inputs[] = {20.0f, 22.0f, 18.0f, 25.0f, 15.0f,
                            20.0f, 21.0f, 19.0f, 20.5f, 20.0f};
    const float expected[] = {20.0f, 20.0f, 19.8f, 20.0f, 19.1f, 20.1f};
    float average = -123.0f;

    CHECK(comm_moving_average_init(&window));
    CHECK(!comm_moving_average_get(&window, &average));
    CHECK(average == -123.0f);
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        CHECK(comm_moving_average_update(&window, inputs[i]));
        if (i < 4) {
            CHECK(!comm_moving_average_get(&window, &average));
            CHECK(average == -123.0f);
        } else {
            CHECK(comm_moving_average_get(&window, &average));
            CHECK(fabsf(average - expected[i - 4]) < 0.0001f);
        }
    }
    puts("PASS waits_for_five_then_slides");
}

static void test_start_and_stop_are_not_rejected(void)
{
    comm_moving_average_t window;
    float average;
    CHECK(comm_moving_average_init(&window));
    for (size_t i = 0; i < 5; ++i) {
        CHECK(comm_moving_average_update(&window, 0.0f));
    }
    CHECK(comm_moving_average_update(&window, 20.0f));
    CHECK(comm_moving_average_get(&window, &average));
    CHECK(average == 4.0f);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(comm_moving_average_update(&window, 20.0f));
    }
    const float expected[] = {16.0f, 12.0f, 8.0f, 4.0f, 0.0f};
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        CHECK(comm_moving_average_update(&window, 0.0f));
        CHECK(comm_moving_average_get(&window, &average));
        CHECK(average == expected[i]);
    }
    puts("PASS start_and_stop_are_not_rejected");
}

static void test_nonfinite_does_not_enter_window(void)
{
    comm_moving_average_t window;
    float average = -1.0f;
    CHECK(comm_moving_average_init(&window));
    for (size_t i = 0; i < 4; ++i) {
        CHECK(comm_moving_average_update(&window, 20.0f));
    }
    CHECK(!comm_moving_average_update(&window, NAN));
    CHECK(!comm_moving_average_update(&window, INFINITY));
    CHECK(!comm_moving_average_update(&window, -INFINITY));
    CHECK(!comm_moving_average_get(&window, &average));
    CHECK(average == -1.0f);
    CHECK(comm_moving_average_update(&window, 30.0f));
    CHECK(comm_moving_average_get(&window, &average));
    CHECK(average == 22.0f);
    CHECK(!comm_moving_average_update(&window, NAN));
    CHECK(comm_moving_average_get(&window, &average));
    CHECK(average == 22.0f);
    puts("PASS nonfinite_does_not_enter_window");
}

static void test_null_and_reset(void)
{
    comm_moving_average_t window;
    float average = -1.0f;
    CHECK(!comm_moving_average_init(NULL));
    CHECK(!comm_moving_average_update(NULL, 1.0f));
    CHECK(!comm_moving_average_get(NULL, &average));
    CHECK(average == -1.0f);
    CHECK(comm_moving_average_init(&window));
    for (size_t i = 0; i < 5; ++i) {
        CHECK(comm_moving_average_update(&window, 1.0f));
    }
    CHECK(!comm_moving_average_get(&window, NULL));
    CHECK(comm_moving_average_init(&window));
    CHECK(!comm_moving_average_get(&window, &average));
    CHECK(average == -1.0f);
    puts("PASS null_and_reset");
}

static void test_extreme_history_does_not_poison_new_window(void)
{
    comm_moving_average_t window;
    float average;
    CHECK(comm_moving_average_init(&window));
    for (size_t i = 0; i < 5; ++i) {
        CHECK(comm_moving_average_update(&window, FLT_MAX));
    }
    CHECK(comm_moving_average_get(&window, &average));
    CHECK(isfinite(average));
    CHECK(average == FLT_MAX);
    for (size_t i = 0; i < 5; ++i) {
        CHECK(comm_moving_average_update(&window, 1.0f));
    }
    CHECK(comm_moving_average_get(&window, &average));
    CHECK(average == 1.0f);
    puts("PASS extreme_history_does_not_poison_new_window");
}

static void test_repeated_wraps(void)
{
    comm_moving_average_t window;
    const float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float average;
    CHECK(comm_moving_average_init(&window));
    for (size_t cycle = 0; cycle < 10000; ++cycle) {
        for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
            CHECK(comm_moving_average_update(&window, inputs[i]));
        }
        CHECK(comm_moving_average_get(&window, &average));
        CHECK(average == 3.0f);
    }
    puts("PASS repeated_wraps_10000");
}

int main(void)
{
    test_waits_for_five_then_slides();
    test_start_and_stop_are_not_rejected();
    test_nonfinite_does_not_enter_window();
    test_null_and_reset();
    test_extreme_history_does_not_poison_new_window();
    test_repeated_wraps();
    puts("MOVING_AVERAGE_TESTS=PASS");
    return EXIT_SUCCESS;
}
