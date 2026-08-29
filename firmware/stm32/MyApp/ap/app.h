#ifndef APP_H
#define APP_H

#include "message_type.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

void app_init(
    SPI_HandleTypeDef *vs1003b_spi,
    UART_HandleTypeDef *message_uart,
    I2C_HandleTypeDef *sensor_i2c
);
void app_process(void);

/* 현재는 디버거에서 마지막 버튼 메시지를 확인하기 위한 임시 인터페이스입니다. */
message_type_t app_get_last_message(void);
bool app_protocol_v2_ready(void);

#endif /* APP_H */
