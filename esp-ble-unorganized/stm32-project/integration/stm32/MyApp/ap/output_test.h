#ifndef OUTPUT_TEST_H
#define OUTPUT_TEST_H

#include "stm32f4xx_hal.h"

/* BUTTON_OUTPUT_TEST 빌드에서만 사용합니다. 제품 동작 규칙이 아닙니다. */
void output_test_init(UART_HandleTypeDef *debug_uart, SPI_HandleTypeDef *codec_spi);
void output_test_process(void);

#endif
