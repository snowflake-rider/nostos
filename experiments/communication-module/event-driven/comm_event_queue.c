#include "comm_event_queue.h"

comm_queue_status_t comm_event_queue_init(comm_event_queue_t *queue)
{
    if (queue == NULL) {
        return COMM_QUEUE_INVALID_ARGUMENT;
    }

    /* 아직 쓰지 않은 items는 읽지 않으므로 전체 배열을 지울 필요가 없다. */
    queue->head = 0;
    queue->count = 0;
    return COMM_QUEUE_OK;
}

comm_queue_status_t comm_event_queue_push(comm_event_queue_t *queue,
                                         const comm_message_t *message)
{
    if (queue == NULL || message == NULL) {
        return COMM_QUEUE_INVALID_ARGUMENT;
    }
    if (message->type != COMM_MESSAGE_EVENT ||
        message->data.event.code < COMM_BUTTON_MSG_1 ||
        message->data.event.code > COMM_BUTTON_MSG_3) {
        return COMM_QUEUE_INVALID_MESSAGE;
    }
    if (queue->count == COMM_EVENT_QUEUE_CAPACITY) {
        return COMM_QUEUE_FULL;
    }

    /* head에서 count칸 뒤가 다음 빈칸이다. 메시지 자체를 복사한다. */
    const size_t tail = (queue->head + queue->count) % COMM_EVENT_QUEUE_CAPACITY;
    queue->items[tail] = *message;
    ++queue->count;
    return COMM_QUEUE_OK;
}

comm_queue_status_t comm_event_queue_peek(const comm_event_queue_t *queue,
                                         comm_message_t *message)
{
    if (queue == NULL || message == NULL) {
        return COMM_QUEUE_INVALID_ARGUMENT;
    }
    if (queue->count == 0) {
        return COMM_QUEUE_EMPTY;
    }

    *message = queue->items[queue->head];
    return COMM_QUEUE_OK;
}

comm_queue_status_t comm_event_queue_pop(comm_event_queue_t *queue,
                                        comm_message_t *message)
{
    const comm_queue_status_t status = comm_event_queue_peek(queue, message);
    if (status != COMM_QUEUE_OK) {
        return status;
    }

    /* 배열 끝에 도착하면 첫 칸으로 돌아가는 ring buffer다. */
    queue->head = (queue->head + 1U) % COMM_EVENT_QUEUE_CAPACITY;
    --queue->count;
    return COMM_QUEUE_OK;
}
