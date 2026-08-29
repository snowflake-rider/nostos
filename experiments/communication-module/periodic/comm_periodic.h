#ifndef COMM_PERIODIC_H
#define COMM_PERIODIC_H

#include "comm_message.h"
#include "moving_average.h"

typedef enum {
    COMM_PERIODIC_OK = 0,
    COMM_PERIODIC_NOT_READY,
    COMM_PERIODIC_NOT_DUE,
    COMM_PERIODIC_STALE,
    COMM_PERIODIC_BUSY,
    COMM_PERIODIC_SENT,
    COMM_PERIODIC_INVALID_ARGUMENT,
    COMM_PERIODIC_INVALID_SAMPLE,
    COMM_PERIODIC_INVALID_TIME
} comm_periodic_status_t;

/*
 * true: 호출 중 데이터를 복사/처리해 접수함. 상대 노드 수신 완료가 아니다.
 * false: 접수하지 못함(BUSY). 포인터를 보관하거나 state에 재진입하지 않는다.
 */
typedef bool (*comm_periodic_send_fn)(const comm_message_t *message, void *context);

/* init 후 사용. 단일 실행 흐름 전용. 공개 멤버는 외부에서 직접 수정하지 않는다. */
typedef struct {
    comm_moving_average_t window;
    uint32_t period_ms;
    uint32_t stale_after_ms;
    uint64_t last_sample_ms;
    uint64_t last_slot_ms;
    uint64_t last_observed_ms;
    bool has_sample;
} comm_periodic_t;

/* 두 간격은 0보다 커야 한다. now_ms는 모든 API에 동일한 단조 증가 시계를 쓴다. */
comm_periodic_status_t comm_periodic_init(comm_periodic_t *state,
                                         uint32_t period_ms,
                                         uint32_t stale_after_ms,
                                         uint64_t now_ms);

/* 새 측정마다 한 번만 호출. 음수/NaN/Inf는 버리고 이전 정상 샘플은 유지한다. */
comm_periodic_status_t comm_periodic_update(comm_periodic_t *state,
                                           float speed_cm_s, uint64_t now_ms);

/* 연결 끊김/센서 오류가 확정되면 윈도를 비우고 다시 5개를 기다린다. */
comm_periodic_status_t comm_periodic_invalidate(comm_periodic_t *state, uint64_t now_ms);

/*
 * OK: 유효한 5개 평균. NOT_READY/STALE: average=0, valid=false.
 * 인자/시간 오류: 출력값은 변경하지 않는다. read는 전송 일정을 소비하지 않는다.
 */
comm_periodic_status_t comm_periodic_read(comm_periodic_t *state, uint64_t now_ms,
                                         comm_speed_data_t *speed);

/* 한 번에 최대 한 건. 지연된 주기는 건너뛰며 BUSY이면 일정은 소비하지 않는다. */
comm_periodic_status_t comm_periodic_poll(comm_periodic_t *state, uint64_t now_ms,
                                         comm_periodic_send_fn send, void *context);

#endif
