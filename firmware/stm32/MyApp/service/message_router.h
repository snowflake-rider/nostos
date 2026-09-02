#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "message_type.h"
#include "stm32f4xx_hal.h"

#include <stdint.h>

/* 카운터를 초기화합니다. 출력 및 UART 초기화는 각 서비스가 담당합니다. */
void message_router_init(void);

/* Publishes one local button/safety event through the framed UART service. */
HAL_StatusTypeDef message_router_publish_local(message_type_t message);

uint32_t message_router_get_local_count(void);

#endif /* MESSAGE_ROUTER_H */
