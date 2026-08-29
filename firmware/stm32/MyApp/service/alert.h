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

/* MPU6050 캘리브레이션 성공 뒤에만 REAR_SAFE 초록 표시를 허용합니다. */
void alert_set_rear_safe_enabled(bool enabled);

/* 외부 안전 메시지에 맞는 LED 상태를 선택합니다. 버튼 메시지는 무시합니다. */
void alert_show(message_type_t message);

/* 로컬 버튼의 의미 색상을 2초간 표시한 뒤 현재 안전 표시로 복귀합니다. */
void alert_show_local_button(message_type_t message);

/* HAL_GetTick()을 기준으로 경고 LED를 비차단 점멸합니다. */
void alert_process(void);

alert_state_t alert_get_state(void);
bool alert_is_led_on(void);

#endif /* ALERT_H */
