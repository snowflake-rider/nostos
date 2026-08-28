#ifndef LAYER8_UART_DIAG_H
#define LAYER8_UART_DIAG_H
#include "driver/uart.h"

/* Read-only, task context. Does not reconfigure pins or consume UART bytes. */
void uart_diag_log_status(uart_port_t port, int tx_gpio, int expected_rx_gpio);
#endif
