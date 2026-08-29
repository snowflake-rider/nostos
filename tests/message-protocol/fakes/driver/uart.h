#ifndef NOSTOS_TEST_ESP_UART_H
#define NOSTOS_TEST_ESP_UART_H
#include "esp_err.h"
#include "freertos/queue.h"
#include <stddef.h>
#define UART_NUM_1 1
#define UART_DATA_8_BITS 3
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
enum { UART_DATA, UART_FIFO_OVF, UART_BUFFER_FULL, UART_PARITY_ERR, UART_FRAME_ERR };
typedef struct { int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk; } uart_config_t;
typedef struct { int type; size_t size; } uart_event_t;
esp_err_t uart_param_config(int port,const uart_config_t *cfg);
esp_err_t uart_set_pin(int port,int tx,int rx,int rts,int cts);
esp_err_t uart_driver_install(int port,int rx,int tx,int event_count,QueueHandle_t *queue,int flags);
esp_err_t uart_driver_delete(int port);
int uart_tx_chars(int port,const char *bytes,uint32_t length);
esp_err_t uart_wait_tx_done(int port,TickType_t wait);
int uart_read_bytes(int port,void *bytes,uint32_t length,TickType_t wait);
esp_err_t uart_flush_input(int port);
#endif
