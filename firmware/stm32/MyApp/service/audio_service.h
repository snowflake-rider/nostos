#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include "message_type.h"
#include "vs1003b.h"

#include <stdbool.h>
#include <stdint.h>

/* 메시지에 대응하는 음원을 선택해 비차단 재생을 시작합니다. */
vs1003b_status_t audio_service_play(message_type_t message);

/* app_process()에서 반복 호출하여 오디오 데이터를 조금씩 전송합니다. */
vs1003b_status_t audio_service_process(void);

/* 디버깅 및 상태 표시용 재생 정보입니다. */
bool audio_service_is_playing(void);
uint32_t audio_service_position(void);

#endif /* AUDIO_SERVICE_H */
