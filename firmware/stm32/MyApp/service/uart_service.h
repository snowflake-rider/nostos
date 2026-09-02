#ifndef UART_SERVICE_H
#define UART_SERVICE_H

#include "message_type.h"
#include "stm32f4xx_hal.h"

#include <stdint.h>

/* Starts byte-wise ISR reception for the framed A5 5A protocol. */
HAL_StatusTypeDef uart_service_init(UART_HandleTypeDef *huart);

/* Maps a local button/safety event into a framed application message. */
HAL_StatusTypeDef uart_service_send_message(message_type_t message);

/* Clears the parser, queued bytes, retries, and RAM-only duplicate history. */
void uart_service_clear_pending(void);

HAL_StatusTypeDef uart_service_get_status(void);
uint32_t uart_service_get_tx_count(void);
uint32_t uart_service_get_rx_count(void);
uint32_t uart_service_get_invalid_count(void);
uint32_t uart_service_get_dropped_count(void);

#endif /* UART_SERVICE_H */
