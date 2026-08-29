#ifndef UART_SERVICE_H
#define UART_SERVICE_H

#include "message_type.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* 1바이트 인터럽트 수신을 시작합니다. */
HAL_StatusTypeDef uart_service_init(UART_HandleTypeDef *huart);

/* 성공한 송신 바이트를 별도 USB 진단 UART에 복사합니다. NULL로 끕니다. */
void uart_service_set_tx_trace(UART_HandleTypeDef *huart);

/* message_type_t를 명시적으로 uint8_t로 변환하여 1바이트 전송합니다. */
HAL_StatusTypeDef uart_service_send_message(message_type_t message);

/* 수신된 유효 메시지가 있으면 true를 반환하고 메시지를 꺼냅니다. */
bool uart_service_get_message(message_type_t *message);

HAL_StatusTypeDef uart_service_get_status(void);
uint32_t uart_service_get_tx_count(void);
uint32_t uart_service_get_rx_count(void);
uint32_t uart_service_get_invalid_count(void);
uint32_t uart_service_get_dropped_count(void);

#endif /* UART_SERVICE_H */
