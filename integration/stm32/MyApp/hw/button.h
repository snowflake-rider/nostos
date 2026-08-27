#ifndef BUTTON_H
#define BUTTON_H

#include "message_type.h"

/* 버튼 상태와 디바운싱 기준 시각을 초기화합니다. */
void button_init(void);

/*
 * 버튼이 안정적으로 눌린 순간에 해당 메시지를 한 번 반환합니다.
 * 새 버튼 이벤트가 없으면 MSG_NONE을 반환합니다.
 */
message_type_t button_get_message(void);

#endif /* BUTTON_H */
