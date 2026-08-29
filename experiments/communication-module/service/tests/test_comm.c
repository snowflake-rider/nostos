#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "comm.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct {
    bool ready;
    size_t attempts;
    size_t count;
    comm_message_t last_attempt;
    comm_message_t accepted[128];
} fake_transport_t;

static bool fake_send(const comm_message_t *message, void *context)
{
    fake_transport_t *transport = context;
    CHECK(message != NULL);
    ++transport->attempts;
    transport->last_attempt = *message;
    if (!transport->ready) {
        return false;
    }
    CHECK(transport->count < sizeof(transport->accepted) / sizeof(transport->accepted[0]));
    transport->accepted[transport->count++] = *message;
    return true;
}

static comm_config_t config_for(fake_transport_t *transport)
{
    return (comm_config_t){
        .speed_period_ms = 200,
        .speed_stale_after_ms = 1000,
        .max_event_burst = 2,
        .send = fake_send,
        .send_context = transport
    };
}

static void expect_button(const fake_transport_t *transport, size_t index, uint8_t code)
{
    CHECK(index < transport->count);
    CHECK(transport->accepted[index].type == COMM_MESSAGE_EVENT);
    CHECK(transport->accepted[index].data.event.code == code);
}

static void test_event_only_fifo_busy_and_one_attempt(void)
{
    comm_t comm;
    fake_transport_t transport = {0};
    const comm_config_t config = config_for(&transport);
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    CHECK(comm_process(&comm, 0) == COMM_IDLE);
    CHECK(transport.attempts == 0);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
    CHECK(transport.attempts == 0);
    CHECK(comm_process(&comm, 0) == COMM_BUSY);
    CHECK(transport.attempts == 1);
    CHECK(transport.count == 0);
    transport.ready = true;
    CHECK(comm_process(&comm, 0) == COMM_EVENT_ACCEPTED);
    CHECK(transport.attempts == 2);
    expect_button(&transport, 0, 1);
    CHECK(comm_process(&comm, 0) == COMM_EVENT_ACCEPTED);
    CHECK(transport.attempts == 3);
    expect_button(&transport, 1, 2);
    CHECK(comm_process(&comm, 0) == COMM_EVENT_ACCEPTED);
    CHECK(transport.attempts == 4);
    expect_button(&transport, 2, 3);
    CHECK(comm_process(&comm, 100) == COMM_IDLE);
    CHECK(transport.attempts == 4);
    puts("PASS event_only_fifo_busy_and_one_attempt");
}

static void fill_speed(comm_t *comm, float value, uint64_t first_ms)
{
    for (uint64_t i = 0; i < 5; ++i) {
        CHECK(comm_update_speed(comm, value, first_ms + i * 10) == COMM_OK);
    }
}

static void expect_speed(const fake_transport_t *transport, size_t index, float average)
{
    CHECK(index < transport->count);
    CHECK(transport->accepted[index].type == COMM_MESSAGE_SPEED);
    CHECK(transport->accepted[index].data.speed.valid);
    CHECK(transport->accepted[index].data.speed.average_cm_s == average);
}

static void test_events_have_bounded_priority_over_due_speed(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    fill_speed(&comm, 20.0f, 0);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
    CHECK(comm_process(&comm, 200) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 0, 1);
    CHECK(comm_process(&comm, 201) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 1, 2);
    CHECK(comm_process(&comm, 202) == COMM_SPEED_ACCEPTED);
    expect_speed(&transport, 2, 20.0f);
    CHECK(comm_process(&comm, 203) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 3, 3);
    CHECK(transport.attempts == 4);
    CHECK(comm_process(&comm, 399) == COMM_IDLE);
    CHECK(transport.attempts == 4);
    CHECK(comm_process(&comm, 400) == COMM_SPEED_ACCEPTED);
    expect_speed(&transport, 4, 20.0f);
    CHECK(transport.attempts == 5);
    puts("PASS events_have_bounded_priority_over_due_speed");
}

static void test_local_speed_read_valid_zero_and_disconnect(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    comm_speed_data_t speed = {.average_cm_s = 99.0f, .valid = true};
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    CHECK(comm_read_speed(&comm, 0, &speed) == COMM_NOT_READY);
    CHECK(!speed.valid && speed.average_cm_s == 0.0f);
    for (uint64_t i = 0; i < 4; ++i) {
        CHECK(comm_update_speed(&comm, 0.0f, i * 10) == COMM_OK);
        CHECK(comm_read_speed(&comm, i * 10, &speed) == COMM_NOT_READY);
        CHECK(!speed.valid);
    }
    CHECK(comm_update_speed(&comm, 0.0f, 40) == COMM_OK);
    CHECK(comm_read_speed(&comm, 200, &speed) == COMM_OK);
    CHECK(speed.valid && speed.average_cm_s == 0.0f);
    CHECK(transport.attempts == 0);
    CHECK(comm_process(&comm, 200) == COMM_SPEED_ACCEPTED);
    expect_speed(&transport, 0, 0.0f);
    CHECK(comm_invalidate_speed(&comm, 210) == COMM_OK);
    CHECK(comm_read_speed(&comm, 210, &speed) == COMM_NOT_READY);
    CHECK(!speed.valid);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
    CHECK(comm_process(&comm, 400) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, 400) == COMM_IDLE);
    CHECK(transport.attempts == 2);
    fill_speed(&comm, 100.0f, 410);
    CHECK(comm_process(&comm, 450) == COMM_SPEED_ACCEPTED);
    expect_speed(&transport, 2, 100.0f);
    puts("PASS local_speed_read_valid_zero_and_disconnect");
}

static void test_busy_preserves_priority_and_retries_latest_speed(void)
{
    comm_t comm;
    fake_transport_t transport = {0};
    const comm_config_t config = config_for(&transport);
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    fill_speed(&comm, 20.0f, 0);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
    CHECK(comm_process(&comm, 200) == COMM_BUSY);
    CHECK(comm_process(&comm, 200) == COMM_BUSY);
    CHECK(transport.attempts == 2 && transport.count == 0);
    transport.ready = true;
    CHECK(comm_process(&comm, 200) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, 200) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 0, 1);
    expect_button(&transport, 1, 2);
    transport.ready = false;
    CHECK(comm_process(&comm, 200) == COMM_BUSY);
    CHECK(transport.attempts == 5 && transport.count == 2);
    CHECK(transport.last_attempt.type == COMM_MESSAGE_SPEED);
    CHECK(transport.last_attempt.data.speed.average_cm_s == 20.0f);
    CHECK(comm_update_speed(&comm, 30.0f, 210) == COMM_OK);
    CHECK(comm_process(&comm, 210) == COMM_BUSY);
    CHECK(transport.attempts == 6 && transport.count == 2);
    CHECK(transport.last_attempt.data.speed.average_cm_s == 22.0f);
    transport.ready = true;
    CHECK(comm_process(&comm, 210) == COMM_SPEED_ACCEPTED);
    expect_speed(&transport, 2, 22.0f);
    CHECK(comm_process(&comm, 210) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 3, 3);
    CHECK(comm_process(&comm, 210) == COMM_IDLE);
    CHECK(transport.attempts == 8);
    puts("PASS busy_preserves_priority_and_retries_latest_speed");
}

static void test_continuous_load_gives_both_types_a_turn(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    comm_config_t config = config_for(&transport);
    config.speed_period_ms = 1;
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    fill_speed(&comm, 20.0f, 0);
    for (size_t i = 0; i < 8; ++i) {
        CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    }
    /* 매 process마다 속도가 due이고, 큐도 계속 채워지는 조건이다. */
    for (size_t i = 0; i < 90; ++i) {
        const comm_status_t status = comm_process(&comm, 200 + (uint64_t)i);
        CHECK(transport.attempts == i + 1);
        if (i % 3 == 2) {
            CHECK(status == COMM_SPEED_ACCEPTED);
            expect_speed(&transport, i, 20.0f);
        } else {
            CHECK(status == COMM_EVENT_ACCEPTED);
            expect_button(&transport, i, 1);
            CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
        }
    }
    puts("PASS continuous_load_gives_both_types_a_turn");
}

static void test_queue_full_and_invalid_codes_preserve_fifo(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    const comm_button_message_t codes[] = {
        COMM_BUTTON_MSG_1, COMM_BUTTON_MSG_2, COMM_BUTTON_MSG_3, COMM_BUTTON_MSG_3,
        COMM_BUTTON_MSG_2, COMM_BUTTON_MSG_1, COMM_BUTTON_MSG_1, COMM_BUTTON_MSG_3
    };
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    CHECK(comm_post_button(&comm, (comm_button_message_t)-1) == COMM_INVALID_MESSAGE);
    CHECK(comm_post_button(&comm, (comm_button_message_t)0) == COMM_INVALID_MESSAGE);
    CHECK(comm_post_button(&comm, (comm_button_message_t)4) == COMM_INVALID_MESSAGE);
    CHECK(comm_post_button(&comm, (comm_button_message_t)257) == COMM_INVALID_MESSAGE);
    for (size_t i = 0; i < 8; ++i) {
        CHECK(comm_post_button(&comm, codes[i]) == COMM_OK);
    }
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_FULL);
    for (size_t i = 0; i < 8; ++i) {
        CHECK(comm_process(&comm, 0) == COMM_EVENT_ACCEPTED);
        expect_button(&transport, i, (uint8_t)codes[i]);
    }
    CHECK(comm_process(&comm, 0) == COMM_IDLE);
    CHECK(transport.attempts == 8);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_process(&comm, 0) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 8, 2);
    puts("PASS queue_full_and_invalid_codes_preserve_fifo");
}

static void test_not_due_and_stale_speed_do_not_block_buttons(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    comm_speed_data_t speed;
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    fill_speed(&comm, 20.0f, 0);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
        CHECK(comm_process(&comm, 199) == COMM_EVENT_ACCEPTED);
    }
    CHECK(comm_process(&comm, 199) == COMM_IDLE);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_process(&comm, 200) == COMM_SPEED_ACCEPTED);
    expect_speed(&transport, 3, 20.0f);
    CHECK(comm_process(&comm, 200) == COMM_EVENT_ACCEPTED);
    CHECK(comm_read_speed(&comm, 1040, &speed) == COMM_STALE);
    CHECK(!speed.valid && speed.average_cm_s == 0.0f);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
        CHECK(comm_process(&comm, 1040) == COMM_EVENT_ACCEPTED);
    }
    CHECK(comm_process(&comm, 1040) == COMM_IDLE);
    CHECK(transport.attempts == 8);
    CHECK(comm_update_speed(&comm, 100.0f, 1041) == COMM_OK);
    CHECK(comm_read_speed(&comm, 1041, &speed) == COMM_NOT_READY);
    CHECK(!speed.valid);
    puts("PASS not_due_and_stale_speed_do_not_block_buttons");
}

static void test_null_and_invalid_config_leave_existing_service_unchanged(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    comm_speed_data_t speed = {.average_cm_s = 99.0f, .valid = true};
    CHECK(comm_init(NULL, &config, 0) == COMM_INVALID_ARGUMENT);
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_init(&comm, NULL, 100) == COMM_INVALID_ARGUMENT);
    comm_config_t bad = config;
    bad.send = NULL;
    CHECK(comm_init(&comm, &bad, 100) == COMM_INVALID_ARGUMENT);
    bad = config;
    bad.speed_period_ms = 0;
    CHECK(comm_init(&comm, &bad, 100) == COMM_INVALID_ARGUMENT);
    bad = config;
    bad.speed_stale_after_ms = 0;
    CHECK(comm_init(&comm, &bad, 100) == COMM_INVALID_ARGUMENT);
    bad = config;
    bad.max_event_burst = 0;
    CHECK(comm_init(&comm, &bad, 100) == COMM_INVALID_ARGUMENT);
    CHECK(comm_post_button(NULL, COMM_BUTTON_MSG_1) == COMM_INVALID_ARGUMENT);
    CHECK(comm_update_speed(NULL, 20.0f, 100) == COMM_INVALID_ARGUMENT);
    CHECK(comm_invalidate_speed(NULL, 100) == COMM_INVALID_ARGUMENT);
    CHECK(comm_read_speed(NULL, 100, &speed) == COMM_INVALID_ARGUMENT);
    CHECK(speed.valid && speed.average_cm_s == 99.0f);
    CHECK(comm_read_speed(&comm, 100, NULL) == COMM_INVALID_ARGUMENT);
    CHECK(comm_process(NULL, 100) == COMM_INVALID_ARGUMENT);
    CHECK(comm_process(&comm, 0) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 0, 1);
    puts("PASS null_and_invalid_config_leave_existing_service_unchanged");
}

static void test_invalid_samples_and_backwards_clock_have_no_send_side_effects(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    comm_speed_data_t speed = {.average_cm_s = 99.0f, .valid = true};
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    fill_speed(&comm, 20.0f, 0);
    CHECK(comm_update_speed(&comm, NAN, 500) == COMM_INVALID_SAMPLE);
    CHECK(comm_update_speed(&comm, INFINITY, 500) == COMM_INVALID_SAMPLE);
    CHECK(comm_update_speed(&comm, -INFINITY, 500) == COMM_INVALID_SAMPLE);
    CHECK(comm_update_speed(&comm, -1.0f, 500) == COMM_INVALID_SAMPLE);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_process(&comm, 39) == COMM_INVALID_TIME);
    CHECK(comm_update_speed(&comm, 100.0f, 39) == COMM_INVALID_TIME);
    CHECK(comm_invalidate_speed(&comm, 39) == COMM_INVALID_TIME);
    CHECK(comm_read_speed(&comm, 39, &speed) == COMM_INVALID_TIME);
    CHECK(speed.valid && speed.average_cm_s == 99.0f);
    CHECK(transport.attempts == 0);
    CHECK(comm_process(&comm, 40) == COMM_EVENT_ACCEPTED);
    CHECK(comm_read_speed(&comm, 40, &speed) == COMM_OK);
    CHECK(speed.valid && speed.average_cm_s == 20.0f);
    CHECK(comm_process(&comm, 200) == COMM_SPEED_ACCEPTED);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_process(&comm, 199) == COMM_INVALID_TIME);
    CHECK(transport.attempts == 2);
    CHECK(comm_process(&comm, 200) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, 600) == COMM_SPEED_ACCEPTED);
    CHECK(transport.count == 4);
    CHECK(comm_process(&comm, 650) == COMM_IDLE);
    CHECK(comm_update_speed(&comm, 30.0f, 649) == COMM_INVALID_TIME);
    CHECK(comm_read_speed(&comm, 1040, &speed) == COMM_STALE);
    CHECK(!speed.valid);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
    CHECK(comm_process(&comm, 1039) == COMM_INVALID_TIME);
    CHECK(comm_invalidate_speed(&comm, 1041) == COMM_OK);
    CHECK(comm_process(&comm, 1040) == COMM_INVALID_TIME);
    CHECK(comm_process(&comm, 1041) == COMM_EVENT_ACCEPTED);
    expect_button(&transport, 4, 3);
    puts("PASS invalid_samples_and_backwards_clock_have_no_send_side_effects");
}

static void test_config_is_copied_reset_and_instances_are_independent(void)
{
    comm_t first;
    comm_t second;
    fake_transport_t one = {.ready = true};
    fake_transport_t two = {.ready = true};
    comm_config_t config = config_for(&one);
    CHECK(comm_init(&first, &config, 0) == COMM_OK);
    config.send_context = &two;
    config.speed_period_ms = 300;
    CHECK(comm_init(&second, &config, 0) == COMM_OK);
    config.send = NULL;
    config.speed_period_ms = 0;
    config.speed_stale_after_ms = 0;
    config.max_event_burst = 0;
    fill_speed(&first, 20.0f, 0);
    fill_speed(&second, 40.0f, 0);
    CHECK(comm_process(&first, 200) == COMM_SPEED_ACCEPTED);
    CHECK(comm_process(&second, 200) == COMM_IDLE);
    CHECK(comm_process(&second, 300) == COMM_SPEED_ACCEPTED);
    expect_speed(&one, 0, 20.0f);
    expect_speed(&two, 0, 40.0f);
    CHECK(comm_post_button(&first, COMM_BUTTON_MSG_1) == COMM_OK);
    config = config_for(&one);
    CHECK(comm_init(&first, &config, 400) == COMM_OK);
    CHECK(comm_process(&first, 400) == COMM_IDLE);
    comm_speed_data_t speed;
    CHECK(comm_read_speed(&first, 400, &speed) == COMM_NOT_READY);
    CHECK(!speed.valid);
    CHECK(comm_read_speed(&second, 400, &speed) == COMM_OK);
    CHECK(speed.average_cm_s == 40.0f);
    fill_speed(&first, 60.0f, 400);
    CHECK(comm_post_button(&first, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_process(&first, 600) == COMM_EVENT_ACCEPTED);
    expect_button(&one, 1, 2);
    CHECK(comm_process(&first, 600) == COMM_SPEED_ACCEPTED);
    expect_speed(&one, 2, 60.0f);
    puts("PASS config_is_copied_reset_and_instances_are_independent");
}

static void test_burst_one_and_missed_slots_do_not_catch_up(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    comm_config_t config = config_for(&transport);
    config.max_event_burst = 1;
    CHECK(comm_init(&comm, &config, 0) == COMM_OK);
    fill_speed(&comm, 20.0f, 0);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_process(&comm, 950) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, 950) == COMM_SPEED_ACCEPTED);
    CHECK(comm_process(&comm, 950) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, 950) == COMM_IDLE);
    CHECK(comm_process(&comm, 999) == COMM_IDLE);
    CHECK(transport.attempts == 3);
    CHECK(comm_process(&comm, 1000) == COMM_SPEED_ACCEPTED);
    expect_button(&transport, 0, 1);
    expect_speed(&transport, 1, 20.0f);
    expect_button(&transport, 2, 2);
    expect_speed(&transport, 3, 20.0f);
    puts("PASS burst_one_and_missed_slots_do_not_catch_up");
}

static void test_near_uint64_max_clock_does_not_overflow(void)
{
    comm_t comm;
    fake_transport_t transport = {.ready = true};
    const comm_config_t config = config_for(&transport);
    const uint64_t start = UINT64_MAX - 500;
    CHECK(comm_init(&comm, &config, start) == COMM_OK);
    fill_speed(&comm, 20.0f, start);
    CHECK(comm_process(&comm, start + 199) == COMM_IDLE);
    CHECK(comm_process(&comm, start + 200) == COMM_SPEED_ACCEPTED);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_1) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_2) == COMM_OK);
    CHECK(comm_post_button(&comm, COMM_BUTTON_MSG_3) == COMM_OK);
    CHECK(comm_process(&comm, UINT64_MAX) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, UINT64_MAX) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, UINT64_MAX) == COMM_SPEED_ACCEPTED);
    CHECK(comm_process(&comm, UINT64_MAX) == COMM_EVENT_ACCEPTED);
    CHECK(comm_process(&comm, UINT64_MAX) == COMM_IDLE);
    CHECK(comm_process(&comm, 0) == COMM_INVALID_TIME);
    CHECK(transport.attempts == 5);
    expect_speed(&transport, 0, 20.0f);
    expect_speed(&transport, 3, 20.0f);
    puts("PASS near_uint64_max_clock_does_not_overflow");
}

int main(void)
{
    test_event_only_fifo_busy_and_one_attempt();
    test_events_have_bounded_priority_over_due_speed();
    test_local_speed_read_valid_zero_and_disconnect();
    test_busy_preserves_priority_and_retries_latest_speed();
    test_continuous_load_gives_both_types_a_turn();
    test_queue_full_and_invalid_codes_preserve_fifo();
    test_not_due_and_stale_speed_do_not_block_buttons();
    test_null_and_invalid_config_leave_existing_service_unchanged();
    test_invalid_samples_and_backwards_clock_have_no_send_side_effects();
    test_config_is_copied_reset_and_instances_are_independent();
    test_burst_one_and_missed_slots_do_not_catch_up();
    test_near_uint64_max_clock_does_not_overflow();
    puts("COMM_SERVICE_TESTS=PASS");
    return EXIT_SUCCESS;
}
