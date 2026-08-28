#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "comm_event_queue.h"

/* 실제 무선 전송 없이 접수 결과만 출력한다. */
static bool mock_send(const comm_message_t *message, bool ready)
{
    if (!ready) {
        printf("MOCK_BUSY code=%u (kept)\n", (unsigned int)message->data.event.code);
        return false;
    }
    printf("MOCK_ACCEPTED code=%u\n", (unsigned int)message->data.event.code);
    return true;
}

int main(void)
{
    comm_event_queue_t queue;
    comm_message_t pending;
    comm_message_t removed;
    const uint8_t codes[] = {COMM_BUTTON_MSG_1, COMM_BUTTON_MSG_2, COMM_BUTTON_MSG_3};

    if (comm_event_queue_init(&queue) != COMM_QUEUE_OK) {
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        const comm_message_t message = {
            .type = COMM_MESSAGE_EVENT,
            .data.event = {.code = codes[i]}
        };
        if (comm_event_queue_push(&queue, &message) != COMM_QUEUE_OK) {
            return EXIT_FAILURE;
        }
        printf("QUEUED code=%u\n", (unsigned int)codes[i]);
    }

    /* BUSY일 때는 pop하지 않는다. 다음 시도에도 첫 메시지가 남는다. */
    if (comm_event_queue_peek(&queue, &pending) != COMM_QUEUE_OK ||
        mock_send(&pending, false)) {
        return EXIT_FAILURE;
    }

    /* 전송 계층이 데이터를 복사해 접수한 경우에만 큐에서 제거한다. */
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        if (comm_event_queue_peek(&queue, &pending) != COMM_QUEUE_OK ||
            pending.data.event.code != codes[i] || !mock_send(&pending, true)) {
            return EXIT_FAILURE;
        }
        if (comm_event_queue_pop(&queue, &removed) != COMM_QUEUE_OK ||
            removed.data.event.code != pending.data.event.code) {
            return EXIT_FAILURE;
        }
    }
    if (comm_event_queue_peek(&queue, &pending) != COMM_QUEUE_EMPTY) {
        return EXIT_FAILURE;
    }
    puts("QUEUE_EMPTY");
    puts("HOST_DEMO=PASS");
    return EXIT_SUCCESS;
}
