#ifndef MESSAGE_SERVICE_H
#define MESSAGE_SERVICE_H

#include "alert.h"
#include "buzzer.h"
#include "message_type.h"
#include "vs1003b.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    vs1003b_status_t audio_status;
    bool audio_playing;
    uint32_t audio_position;
    bool buzzer_active;
    buzzer_pattern_t buzzer_pattern;
    alert_state_t alert_state;
    bool alert_led_on;
} message_service_status_t;

/* 출력 장치를 초기화하고 VS1003B의 초기 상태를 전달받습니다. */
void message_service_init(vs1003b_status_t initial_audio_status);

/* 메시지 의미에 맞게 LED, 음성, 부저 출력을 요청합니다. */
void message_service_handle(message_type_t message);

/* 비차단 오디오 전송과 부저 종료 처리를 진행합니다. */
void message_service_process(void);

/* 디버깅 및 상위 계층 상태 확인용입니다. */
const message_service_status_t *message_service_get_status(void);

#endif /* MESSAGE_SERVICE_H */
