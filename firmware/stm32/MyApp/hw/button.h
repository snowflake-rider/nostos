#ifndef BUTTON_H
#define BUTTON_H

#include "message_type.h"

#include <stdbool.h>

/* 버튼 상태와 디바운싱 기준 시각을 초기화합니다. */
void button_init(void);

/*
 * 버튼이 안정적으로 눌린 순간에 해당 메시지를 한 번 반환합니다.
 * 새 버튼 이벤트가 없으면 MSG_NONE을 반환합니다.
 */
message_type_t button_get_message(void);

/* BTN4가 안정적으로 눌리면 로컬 출력 리셋 요청을 한 번 꺼냅니다. */
bool button_take_output_reset_request(void);

#endif /* BUTTON_H */
