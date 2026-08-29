#ifndef COMM_MESSAGE_H
#define COMM_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

/* 애플리케이션 메시지 종류. UART/BLE 바이트 값으로 직렬화한 규약은 아니다. */
typedef enum {
    COMM_MESSAGE_EVENT = 1,
    COMM_MESSAGE_SPEED = 2
} comm_message_type_t;

/* 버튼의 실제 의미는 센서 담당 팀원과 합의한다. */
typedef enum {
    COMM_BUTTON_MSG_1 = 1,
    COMM_BUTTON_MSG_2 = 2,
    COMM_BUTTON_MSG_3 = 3
} comm_button_message_t;

typedef struct {
    uint8_t code;
} comm_event_data_t;

typedef struct {
    float average_cm_s; /* Head의 최근 5개 측정 평균. 단위: cm/s */
    bool valid;        /* false: 값 없음/만료. true와 평균 0: 측정된 정지 */
} comm_speed_data_t;

/*
 * 메모리에서 사용하는 공통 메시지다. UART/BLE wire packet이 아니다.
 * sizeof(struct) 바이트를 그대로 전송하지 않는다. 인코딩은 별도로 만든다.
 */
typedef struct {
    comm_message_type_t type;
    union {
        comm_event_data_t event;
        comm_speed_data_t speed;
    } data;
} comm_message_t;

#endif
