#include <stdio.h>
#include <stdlib.h>

#include "comm_event_queue.h"

/* Release 빌드의 NDEBUG에서도 검증을 생략하지 않는다. */
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static comm_message_t button(uint8_t code)
{
    const comm_message_t message = {
        .type = COMM_MESSAGE_EVENT,
        .data.event = {.code = code}
    };
    return message;
}

static void test_init_is_empty(void)
{
    comm_event_queue_t queue;
    comm_message_t output = button(COMM_BUTTON_MSG_3);

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_peek(&queue, &output) == COMM_QUEUE_EMPTY);
    CHECK(output.type == COMM_MESSAGE_EVENT);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_3);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_3);
    puts("PASS init_is_empty");
}

static void test_fifo_123(void)
{
    comm_event_queue_t queue;
    const comm_message_t first = button(COMM_BUTTON_MSG_1);
    const comm_message_t second = button(COMM_BUTTON_MSG_2);
    const comm_message_t third = button(COMM_BUTTON_MSG_3);
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &first) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &second) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &third) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.type == COMM_MESSAGE_EVENT);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_2);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_3);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS fifo_123");
}

static void test_invalid_message_preserves_queue(void)
{
    comm_event_queue_t queue;
    const comm_message_t valid = button(COMM_BUTTON_MSG_2);
    const uint8_t invalid_codes[] = {0, 4, UINT8_MAX};
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &valid) == COMM_QUEUE_OK);
    for (size_t i = 0; i < sizeof(invalid_codes) / sizeof(invalid_codes[0]); ++i) {
        const comm_message_t invalid = button(invalid_codes[i]);
        CHECK(comm_event_queue_push(&queue, &invalid) == COMM_QUEUE_INVALID_MESSAGE);
    }

    comm_message_t wrong_type = button(COMM_BUTTON_MSG_1);
    wrong_type.type = (comm_message_type_t)0;
    CHECK(comm_event_queue_push(&queue, &wrong_type) == COMM_QUEUE_INVALID_MESSAGE);
    wrong_type.type = COMM_MESSAGE_SPEED;
    CHECK(comm_event_queue_push(&queue, &wrong_type) == COMM_QUEUE_INVALID_MESSAGE);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_2);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS invalid_message_preserves_queue");
}

static void test_full_preserves_all_eight_items(void)
{
    comm_event_queue_t queue;
    const uint8_t expected[] = {1, 2, 3, 1, 2, 3, 1, 2};
    const comm_message_t extra = button(COMM_BUTTON_MSG_3);
    const comm_message_t invalid = button(0);
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        const comm_message_t message = button(expected[i]);
        CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    }
    CHECK(comm_event_queue_push(&queue, &extra) == COMM_QUEUE_FULL);
    /* 입력 오류는 가득 참 검사보다 먼저 반환한다. */
    CHECK(comm_event_queue_push(&queue, &invalid) == COMM_QUEUE_INVALID_MESSAGE);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
        CHECK(output.data.event.code == expected[i]);
    }
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS full_preserves_all_eight_items");
}

static void test_wraparound_preserves_fifo(void)
{
    comm_event_queue_t queue;
    const uint8_t initial[] = {1, 2, 3, 1, 2, 3, 1, 2};
    const uint8_t added[] = {3, 2, 1, 3, 2};
    const uint8_t expected[] = {3, 1, 2, 3, 2, 1, 3, 2};
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    for (size_t i = 0; i < sizeof(initial) / sizeof(initial[0]); ++i) {
        const comm_message_t message = button(initial[i]);
        CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    }
    for (size_t i = 0; i < 5; ++i) {
        CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
        CHECK(output.data.event.code == initial[i]);
    }
    for (size_t i = 0; i < sizeof(added) / sizeof(added[0]); ++i) {
        const comm_message_t message = button(added[i]);
        CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    }
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
        CHECK(output.data.event.code == expected[i]);
    }
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS wraparound_preserves_fifo");
}

static void test_peek_keeps_pending_until_pop(void)
{
    comm_event_queue_t queue;
    const comm_message_t message = button(COMM_BUTTON_MSG_1);
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_peek(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    /* 전송이 바쁘면 pop하지 않는다. 다시 확인해도 같은 이벤트가 남아 있다. */
    output.data.event.code = COMM_BUTTON_MSG_3;
    CHECK(comm_event_queue_peek(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    CHECK(comm_event_queue_peek(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS peek_keeps_pending_until_pop");
}

static void test_push_copies_input(void)
{
    comm_event_queue_t queue;
    comm_message_t input = button(COMM_BUTTON_MSG_1);
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &input) == COMM_QUEUE_OK);
    input.data.event.code = COMM_BUTTON_MSG_3;
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    puts("PASS push_copies_input");
}

static void test_repeated_code_is_two_events(void)
{
    comm_event_queue_t queue;
    const comm_message_t message = button(COMM_BUTTON_MSG_2);
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_2);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_2);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS repeated_code_is_two_events");
}

static void test_null_arguments_preserve_pending(void)
{
    comm_event_queue_t queue;
    const comm_message_t message = button(COMM_BUTTON_MSG_2);
    comm_message_t output = button(COMM_BUTTON_MSG_3);

    CHECK(comm_event_queue_init(NULL) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(NULL, &message) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(comm_event_queue_push(&queue, NULL) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(comm_event_queue_peek(NULL, &output) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(comm_event_queue_peek(&queue, NULL) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(comm_event_queue_pop(NULL, &output) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(comm_event_queue_pop(&queue, NULL) == COMM_QUEUE_INVALID_ARGUMENT);
    CHECK(output.type == COMM_MESSAGE_EVENT);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_3);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_2);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    puts("PASS null_arguments_preserve_pending");
}

static void test_reinit_discards_pending(void)
{
    comm_event_queue_t queue;
    const comm_message_t message = button(COMM_BUTTON_MSG_1);
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    puts("PASS reinit_discards_pending");
}

static void test_queue_instances_are_independent(void)
{
    comm_event_queue_t first;
    comm_event_queue_t second;
    const comm_message_t one = button(COMM_BUTTON_MSG_1);
    const comm_message_t three = button(COMM_BUTTON_MSG_3);
    comm_message_t output;

    CHECK(comm_event_queue_init(&first) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_init(&second) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&first, &one) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_push(&second, &three) == COMM_QUEUE_OK);
    CHECK(comm_event_queue_pop(&second, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_3);
    CHECK(comm_event_queue_pop(&first, &output) == COMM_QUEUE_OK);
    CHECK(output.data.event.code == COMM_BUTTON_MSG_1);
    puts("PASS queue_instances_are_independent");
}

static void test_repeated_wrap_cycles(void)
{
    comm_event_queue_t queue;
    const uint8_t expected[] = {3, 1, 2};
    comm_message_t output;

    CHECK(comm_event_queue_init(&queue) == COMM_QUEUE_OK);
    for (size_t cycle = 0; cycle < 1000; ++cycle) {
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            const comm_message_t message = button(expected[i]);
            CHECK(comm_event_queue_push(&queue, &message) == COMM_QUEUE_OK);
        }
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_OK);
            CHECK(output.data.event.code == expected[i]);
        }
        CHECK(comm_event_queue_pop(&queue, &output) == COMM_QUEUE_EMPTY);
    }
    puts("PASS repeated_wrap_cycles_1000");
}

int main(void)
{
    test_init_is_empty();
    test_fifo_123();
    test_invalid_message_preserves_queue();
    test_full_preserves_all_eight_items();
    test_wraparound_preserves_fifo();
    test_peek_keeps_pending_until_pop();
    test_push_copies_input();
    test_repeated_code_is_two_events();
    test_null_arguments_preserve_pending();
    test_reinit_discards_pending();
    test_queue_instances_are_independent();
    test_repeated_wrap_cycles();
    puts("EVENT_QUEUE_TESTS=PASS");
    return EXIT_SUCCESS;
}
