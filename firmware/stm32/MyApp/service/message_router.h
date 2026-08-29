#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "message_type.h"
#include "stm32f4xx_hal.h"

#include <stdint.h>

/* 카운터를 초기화합니다. 출력 및 UART 초기화는 각 서비스가 담당합니다. */
void message_router_init(void);

/* 이 보드에서 생성된 메시지: 로컬 출력 후 USART로 한 번 전송합니다. */
HAL_StatusTypeDef message_router_publish_local(message_type_t message);

/* USART로 받은 메시지: 로컬 출력만 하고 다시 전송하지 않습니다. */
void message_router_deliver_remote(message_type_t message);

uint32_t message_router_get_local_count(void);
uint32_t message_router_get_remote_count(void);

#endif /* MESSAGE_ROUTER_H */
