#ifndef COMM_EVENT_QUEUE_H
#define COMM_EVENT_QUEUE_H

#include <stddef.h>

#include "comm_message.h"

/* 첫 학습용 용량. 한 칸을 비우지 않고 8칸 모두 사용한다. */
#define COMM_EVENT_QUEUE_CAPACITY 8U

typedef enum {
    COMM_QUEUE_OK = 0,
    COMM_QUEUE_EMPTY,
    COMM_QUEUE_FULL,
    COMM_QUEUE_INVALID_ARGUMENT,
    COMM_QUEUE_INVALID_MESSAGE
} comm_queue_status_t;

/*
 * 정적/스택 할당을 위해 공개한다. 멤버는 API 밖에서 직접 수정하지 않는다.
 * init() 후 사용한다. 단일 실행 흐름 전용이며 ISR/Task 동시 접근은 금지한다.
 */
typedef struct {
    comm_message_t items[COMM_EVENT_QUEUE_CAPACITY];
    size_t head;  /* 다음에 꺼낼 이벤트가 있는 칸 */
    size_t count; /* 현재 보관 중인 이벤트 수 */
} comm_event_queue_t;

/* 재초기화하면 대기 중인 이벤트도 모두 비운다. */
comm_queue_status_t comm_event_queue_init(comm_event_queue_t *queue);

/* 값을 복사해서 저장한다. OK는 로컬 큐 접수이며 무선 수신 성공이 아니다. */
comm_queue_status_t comm_event_queue_push(comm_event_queue_t *queue,
                                         const comm_message_t *message);

/* 맨 앞 메시지를 복사하되 제거하지 않는다. 실패 시 출력값은 변경하지 않는다. */
comm_queue_status_t comm_event_queue_peek(const comm_event_queue_t *queue,
                                         comm_message_t *message);

/* 맨 앞 메시지를 복사하고 제거한다. 실패 시 큐와 출력값을 변경하지 않는다. */
comm_queue_status_t comm_event_queue_pop(comm_event_queue_t *queue,
                                        comm_message_t *message);

#endif
