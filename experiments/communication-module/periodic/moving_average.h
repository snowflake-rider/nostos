#ifndef COMM_MOVING_AVERAGE_H
#define COMM_MOVING_AVERAGE_H

#include <stdbool.h>
#include <stddef.h>

#define COMM_MOVING_AVERAGE_WINDOW_SIZE 5U

/* 정적 할당용 공개 구조체. 초기화 후 멤버를 직접 변경하지 않는다. */
typedef struct {
    float buffer[COMM_MOVING_AVERAGE_WINDOW_SIZE];
    size_t buffer_idx;
    size_t count;
    double window_sum;
} comm_moving_average_t;

bool comm_moving_average_init(comm_moving_average_t *window);

/* 새 측정마다 한 번 호출한다. NaN/Inf 거절 시 윈도를 변경하지 않는다. */
bool comm_moving_average_update(comm_moving_average_t *window, float input);

/* 5개가 모인 뒤에만 true. false이면 average 출력값을 변경하지 않는다. */
bool comm_moving_average_get(const comm_moving_average_t *window, float *average);

#endif
