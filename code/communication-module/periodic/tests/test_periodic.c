#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "comm_periodic.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_missing_warmup_and_valid_zero(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed = {.average_cm_s = 99.0f, .valid = true};
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_read(&state, 0, &speed) == COMM_PERIODIC_NOT_READY);
    CHECK(!speed.valid);
    CHECK(speed.average_cm_s == 0.0f);
    for (uint64_t i = 0; i < 4; ++i) {
        CHECK(comm_periodic_update(&state, 0.0f, i * 10) == COMM_PERIODIC_OK);
        CHECK(comm_periodic_read(&state, i * 10, &speed) == COMM_PERIODIC_NOT_READY);
        CHECK(!speed.valid);
    }
    CHECK(comm_periodic_update(&state, 0.0f, 40) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_read(&state, 40, &speed) == COMM_PERIODIC_OK);
    CHECK(speed.valid);
    CHECK(speed.average_cm_s == 0.0f);
    puts("PASS missing_warmup_and_valid_zero");
}

typedef struct {
    bool ready;
    size_t attempts;
    size_t accepted;
    comm_message_t last;
} fake_transport_t;

static bool fake_send(const comm_message_t *message, void *context)
{
    fake_transport_t *transport = context;
    CHECK(message != NULL);
    CHECK(message->type == COMM_MESSAGE_SPEED);
    CHECK(message->data.speed.valid);
    ++transport->attempts;
    transport->last = *message;
    if (!transport->ready) {
        return false;
    }
    ++transport->accepted;
    return true;
}

static void test_due_boundary_and_busy_uses_latest_mean(void)
{
    comm_periodic_t state;
    fake_transport_t transport = {0};
    const float values[] = {20.0f, 22.0f, 18.0f, 25.0f, 15.0f};
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_poll(&state, 0, fake_send, &transport) == COMM_PERIODIC_NOT_READY);
    CHECK(transport.attempts == 0);
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        CHECK(comm_periodic_update(&state, values[i], (uint64_t)i * 10) == COMM_PERIODIC_OK);
    }
    CHECK(comm_periodic_poll(&state, 199, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(transport.attempts == 0);
    CHECK(comm_periodic_poll(&state, 200, fake_send, &transport) == COMM_PERIODIC_BUSY);
    CHECK(transport.last.data.speed.average_cm_s == 20.0f);
    CHECK(transport.accepted == 0);
    CHECK(comm_periodic_update(&state, 30.0f, 210) == COMM_PERIODIC_OK);
    transport.ready = true;
    CHECK(comm_periodic_poll(&state, 210, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.last.data.speed.average_cm_s == 22.0f);
    CHECK(transport.attempts == 2);
    CHECK(transport.accepted == 1);
    CHECK(comm_periodic_poll(&state, 210, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(comm_periodic_poll(&state, 399, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(comm_periodic_poll(&state, 400, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.accepted == 2);
    puts("PASS due_boundary_and_busy_uses_latest_mean");
}

static void fill_window(comm_periodic_t *state, float value, uint64_t first_ms)
{
    for (uint64_t i = 0; i < 5; ++i) {
        CHECK(comm_periodic_update(state, value, first_ms + i * 10) == COMM_PERIODIC_OK);
    }
}

static void test_disconnect_requires_five_new_samples(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed;
    fake_transport_t transport = {.ready = true};
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, 0);
    CHECK(comm_periodic_invalidate(&state, 50) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_read(&state, 50, &speed) == COMM_PERIODIC_NOT_READY);
    CHECK(!speed.valid);
    CHECK(comm_periodic_poll(&state, 200, fake_send, &transport) == COMM_PERIODIC_NOT_READY);
    CHECK(transport.attempts == 0);
    fill_window(&state, 100.0f, 210);
    CHECK(comm_periodic_poll(&state, 250, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.last.data.speed.average_cm_s == 100.0f);
    puts("PASS disconnect_requires_five_new_samples");
}

static void test_expiry_boundary_and_recovery(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed;
    fake_transport_t transport = {.ready = true};
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, 0);
    CHECK(comm_periodic_read(&state, 1039, &speed) == COMM_PERIODIC_OK);
    CHECK(speed.valid);
    CHECK(comm_periodic_poll(&state, 1040, fake_send, &transport) == COMM_PERIODIC_STALE);
    CHECK(comm_periodic_read(&state, 1040, &speed) == COMM_PERIODIC_STALE);
    CHECK(!speed.valid);
    CHECK(speed.average_cm_s == 0.0f);
    CHECK(transport.attempts == 0);
    CHECK(comm_periodic_update(&state, 100.0f, 1040) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_read(&state, 1040, &speed) == COMM_PERIODIC_NOT_READY);
    CHECK(!speed.valid);
    for (uint64_t i = 1; i < 5; ++i) {
        CHECK(comm_periodic_update(&state, 100.0f, 1040 + i * 10) == COMM_PERIODIC_OK);
    }
    CHECK(comm_periodic_poll(&state, 1080, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.last.data.speed.average_cm_s == 100.0f);
    puts("PASS expiry_boundary_and_recovery");
}

static void test_gap_resets_without_prior_poll(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed;
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, 0);
    CHECK(comm_periodic_update(&state, 100.0f, 1040) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_read(&state, 1040, &speed) == COMM_PERIODIC_NOT_READY);
    CHECK(!speed.valid);
    puts("PASS gap_resets_without_prior_poll");
}

static void test_delayed_poll_skips_old_slots(void)
{
    comm_periodic_t state;
    fake_transport_t transport = {.ready = true};
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, 0);
    CHECK(comm_periodic_poll(&state, 950, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.accepted == 1);
    CHECK(comm_periodic_poll(&state, 950, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(comm_periodic_poll(&state, 999, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(transport.attempts == 1);
    CHECK(comm_periodic_poll(&state, 1000, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.accepted == 2);
    puts("PASS delayed_poll_skips_old_slots");
}

static void test_poll_does_not_synthesize_samples(void)
{
    comm_periodic_t state;
    fake_transport_t transport = {.ready = true};
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    for (uint64_t now = 0; now <= 1000; now += 200) {
        CHECK(comm_periodic_poll(&state, now, fake_send, &transport) == COMM_PERIODIC_NOT_READY);
    }
    for (uint64_t i = 0; i < 4; ++i) {
        CHECK(comm_periodic_update(&state, 0.0f, 1100 + i * 10) == COMM_PERIODIC_OK);
    }
    for (uint64_t now = 1200; now <= 1500; now += 100) {
        CHECK(comm_periodic_poll(&state, now, fake_send, &transport) == COMM_PERIODIC_NOT_READY);
    }
    CHECK(transport.attempts == 0);
    CHECK(comm_periodic_update(&state, 0.0f, 1500) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_poll(&state, 1500, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(transport.last.data.speed.valid);
    CHECK(transport.last.data.speed.average_cm_s == 0.0f);
    puts("PASS poll_does_not_synthesize_samples");
}

static void test_invalid_samples_do_not_refresh_freshness(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed;
    CHECK(comm_periodic_init(&state, 200, 1000, 0) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, 0);
    CHECK(comm_periodic_update(&state, -1.0f, 100) == COMM_PERIODIC_INVALID_SAMPLE);
    CHECK(comm_periodic_update(&state, NAN, 100) == COMM_PERIODIC_INVALID_SAMPLE);
    CHECK(comm_periodic_update(&state, INFINITY, 10000) == COMM_PERIODIC_INVALID_SAMPLE);
    CHECK(comm_periodic_update(&state, -INFINITY, 10000) == COMM_PERIODIC_INVALID_SAMPLE);
    CHECK(comm_periodic_read(&state, 100, &speed) == COMM_PERIODIC_OK);
    CHECK(speed.average_cm_s == 20.0f);
    CHECK(comm_periodic_read(&state, 1040, &speed) == COMM_PERIODIC_STALE);
    CHECK(!speed.valid);
    puts("PASS invalid_samples_do_not_refresh_freshness");
}

static void test_invalid_arguments_and_time_preserve_state(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed = {.average_cm_s = 123.0f, .valid = true};
    fake_transport_t transport = {.ready = true};
    CHECK(comm_periodic_init(NULL, 200, 1000, 100) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_init(&state, 200, 1000, 100) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, 100);
    CHECK(comm_periodic_init(&state, 0, 1000, 0) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_init(&state, 200, 0, 0) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_update(NULL, 20.0f, 140) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_read(NULL, 140, &speed) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_read(&state, 140, NULL) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_invalidate(NULL, 140) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_poll(NULL, 140, fake_send, &transport) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_poll(&state, 140, NULL, &transport) == COMM_PERIODIC_INVALID_ARGUMENT);
    CHECK(comm_periodic_update(&state, 100.0f, 139) == COMM_PERIODIC_INVALID_TIME);
    CHECK(comm_periodic_invalidate(&state, 139) == COMM_PERIODIC_INVALID_TIME);
    CHECK(comm_periodic_read(&state, 139, &speed) == COMM_PERIODIC_INVALID_TIME);
    CHECK(speed.average_cm_s == 123.0f);
    CHECK(speed.valid);
    CHECK(comm_periodic_poll(&state, 139, fake_send, &transport) == COMM_PERIODIC_INVALID_TIME);
    CHECK(transport.attempts == 0);
    CHECK(comm_periodic_read(&state, 140, &speed) == COMM_PERIODIC_OK);
    CHECK(speed.average_cm_s == 20.0f);
    CHECK(comm_periodic_poll(&state, 299, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(comm_periodic_poll(&state, 300, fake_send, &transport) == COMM_PERIODIC_SENT);
    puts("PASS invalid_arguments_and_time_preserve_state");
}

static void test_large_clock_has_no_deadline_overflow(void)
{
    comm_periodic_t state;
    fake_transport_t transport = {.ready = true};
    const uint64_t start = UINT64_MAX - 1000;
    CHECK(comm_periodic_init(&state, 200, 2000, start) == COMM_PERIODIC_OK);
    fill_window(&state, 20.0f, start);
    CHECK(comm_periodic_poll(&state, start + 199, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(comm_periodic_poll(&state, start + 200, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(comm_periodic_poll(&state, UINT64_MAX, fake_send, &transport) == COMM_PERIODIC_SENT);
    CHECK(comm_periodic_poll(&state, UINT64_MAX, fake_send, &transport) == COMM_PERIODIC_NOT_DUE);
    CHECK(comm_periodic_poll(&state, 0, fake_send, &transport) == COMM_PERIODIC_INVALID_TIME);
    CHECK(transport.accepted == 2);
    puts("PASS large_clock_has_no_deadline_overflow");
}

static void test_instances_are_independent(void)
{
    comm_periodic_t first;
    comm_periodic_t second;
    comm_speed_data_t speed;
    CHECK(comm_periodic_init(&first, 200, 1000, 0) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_init(&second, 300, 1000, 0) == COMM_PERIODIC_OK);
    fill_window(&first, 20.0f, 0);
    fill_window(&second, 100.0f, 0);
    CHECK(comm_periodic_invalidate(&first, 50) == COMM_PERIODIC_OK);
    CHECK(comm_periodic_read(&first, 50, &speed) == COMM_PERIODIC_NOT_READY);
    CHECK(comm_periodic_read(&second, 50, &speed) == COMM_PERIODIC_OK);
    CHECK(speed.average_cm_s == 100.0f);
    puts("PASS instances_are_independent");
}

int main(void)
{
    test_missing_warmup_and_valid_zero();
    test_due_boundary_and_busy_uses_latest_mean();
    test_disconnect_requires_five_new_samples();
    test_expiry_boundary_and_recovery();
    test_gap_resets_without_prior_poll();
    test_delayed_poll_skips_old_slots();
    test_poll_does_not_synthesize_samples();
    test_invalid_samples_do_not_refresh_freshness();
    test_invalid_arguments_and_time_preserve_state();
    test_large_clock_has_no_deadline_overflow();
    test_instances_are_independent();
    puts("PERIODIC_TESTS=PASS");
    return EXIT_SUCCESS;
}
