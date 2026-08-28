#ifndef COMM_H
#define COMM_H

#include "comm_event_queue.h"
#include "comm_periodic.h"

typedef enum {
    COMM_OK = 0,
    COMM_FULL,
    COMM_NOT_READY,
    COMM_STALE,
    COMM_IDLE,
    COMM_BUSY,
    COMM_EVENT_ACCEPTED,
    COMM_SPEED_ACCEPTED,
    COMM_INVALID_ARGUMENT,
    COMM_INVALID_MESSAGE,
    COMM_INVALID_SAMPLE,
    COMM_INVALID_TIME
} comm_status_t;

/*
 * true: 호출 중 메시지를 복사/처리해 로컬 전송부가 접수함. 원격 ACK가 아니다.
 * false: 아무것도 접수하지 못함(BUSY). 같은 process에서 다른 종류를 보내지 않는다.
 * 빠르게 반환하고, 임시 message 포인터 보관/같은 comm의 API 재진입은 금지한다.
 */
typedef bool (*comm_send_fn)(const comm_message_t *message, void *context);

typedef struct {
    uint32_t speed_period_ms;
    uint32_t speed_stale_after_ms;
    uint32_t max_event_burst; /* 모두 0보다 커야 한다. 기본값은 강제하지 않는다. */
    comm_send_fn send;
    void *send_context;      /* NULL 가능. 사용하는 객체는 comm보다 오래 살아야 한다. */
} comm_config_t;

/*
 * 정적/스택 할당용 공개 저장 공간. 반드시 init 후 사용하고 멤버는 직접 만지지 않는다.
 * 내부 queue/periodic API도 직접 호출하지 않는다. 단일 실행 흐름용이며 ISR-safe가 아니다.
 */
typedef struct {
    comm_event_queue_t events;
    comm_periodic_t speed;
    comm_send_fn send;
    void *send_context;
    uint32_t max_event_burst;
    uint32_t events_since_speed;
} comm_t;

/* config를 복사한다. 재초기화하면 큐/평균/일정이 초기화되며 대기 이벤트는 사라진다. */
comm_status_t comm_init(comm_t *comm, const comm_config_t *config, uint64_t now_ms);

/* OK는 8칸 로컬 큐 접수, FULL은 미접수. 버튼 코드는 1, 2, 3만 받는다. */
comm_status_t comm_post_button(comm_t *comm, comm_button_message_t code);

/* Head의 새 측정마다 한 번만 추가. 단위 cm/s. 이전 평균을 재입력하지 않는다. */
comm_status_t comm_update_speed(comm_t *comm, float speed_cm_s, uint64_t now_ms);
comm_status_t comm_invalidate_speed(comm_t *comm, uint64_t now_ms);

/* Head의 로컬 평균이다(B/C 수신 API 아님). NOT_READY/STALE이면 valid=false. */
comm_status_t comm_read_speed(comm_t *comm, uint64_t now_ms, comm_speed_data_t *speed);

/*
 * main loop 또는 한 Task에서 반복 호출. 한 번에 콜백 최대 1회, 대기/반복 송신 없음.
 * 이벤트를 먼저 보내되 max_event_burst건 접수 후에는 due인 유효 속도에 우선권을 준다.
 * BUSY에서는 이벤트/속도 일정/우선권을 소비하지 않는다. IDLE은 지금 보낼 것이 없음.
 * now_ms는 update/read/invalidate와 동일한 단조 증가 64-bit 밀리초 시계다.
 */
comm_status_t comm_process(comm_t *comm, uint64_t now_ms);

#endif
