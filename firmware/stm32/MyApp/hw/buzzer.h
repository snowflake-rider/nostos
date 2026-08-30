#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BUZZER_PATTERN_NONE = 0,
    BUZZER_PATTERN_EMERGENCY
} buzzer_pattern_t;

/* 액티브 부저를 꺼진 상태로 초기화합니다. */
void buzzer_init(void);

/* 긴급 경고 패턴을 비차단 방식으로 시작합니다. */
void buzzer_play_pattern(buzzer_pattern_t pattern);

/* 진행 중인 패턴을 즉시 중단합니다. */
void buzzer_stop(void);

/* 경과 시간을 확인해 부저를 자동으로 끕니다. */
void buzzer_process(void);

/* 현재 부저 출력 상태를 반환합니다. */
bool buzzer_is_active(void);
buzzer_pattern_t buzzer_get_pattern(void);

#endif /* BUZZER_H */
