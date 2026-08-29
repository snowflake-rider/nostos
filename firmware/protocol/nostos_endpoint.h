#ifndef NOSTOS_ENDPOINT_H
#define NOSTOS_ENDPOINT_H
#include "nostos_state.h"
#include "nostos_uart.h"
#define NOSTOS_REQUEST_MAX_AGE_MS 2000U
/* All callbacks execute in main/task context, never an ISR. */
typedef struct {
    void *context;
    bool (*uart_send)(void *context, const uint8_t *frame, size_t length);
    void (*outputs)(void *context, nostos_outputs_t outputs);
    bool (*audio_ready)(void *context);
    bool (*audio_play)(void *context, uint8_t type);
} nostos_endpoint_io_t;
typedef struct {
    nostos_receiver_t receiver;
    nostos_sender_t sender;
    nostos_uart_parser_t uart;
    nostos_endpoint_io_t io;
    nostos_outputs_t last_outputs;
    bool outputs_initialized;
    uint32_t expired_requests, audio_failures;
} nostos_endpoint_t;
nostos_result_t nostos_endpoint_init(nostos_endpoint_t *endpoint, uint8_t source,
    uint32_t session, const nostos_endpoint_io_t *io);
nostos_result_t nostos_endpoint_uart_byte(nostos_endpoint_t *endpoint, uint8_t byte, uint32_t now_ms);
/* Stamps a NEW message, applies canonical local state, then sends a UART frame.
 * IO_ERROR can mean a partial physical write: do not blindly resend as new. */
nostos_result_t nostos_endpoint_publish(nostos_endpoint_t *endpoint, nostos_message_t *message, uint32_t now_ms);
void nostos_endpoint_process(nostos_endpoint_t *endpoint, uint32_t now_ms);
#endif
