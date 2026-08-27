#ifndef ALERT_H
#define ALERT_H

#include "message_type.h"

#include <stdbool.h>

typedef enum
{
    ALERT_STATE_OFF = 0,
    ALERT_STATE_REAR_SAFE,
    ALERT_STATE_REAR_WARNING,
    ALERT_STATE_EMERGENCY
} alert_state_t;

/* 경고 출력 장치를 안전한 초기 상태로 만듭니다. */
void alert_init(void);

/* 외부 안전 메시지에 맞는 LED 상태를 선택합니다. 버튼 메시지는 무시합니다. */
void alert_show(message_type_t message);

/* HAL_GetTick()을 기준으로 경고 LED를 비차단 점멸합니다. */
void alert_process(void);

alert_state_t alert_get_state(void);
bool alert_is_led_on(void);

#endif /* ALERT_H */
