#ifndef NOSTOS_UART_H
#define NOSTOS_UART_H
#include "nostos_protocol.h"
#define NOSTOS_UART_FLAG 0x7eU
#define NOSTOS_UART_ESCAPE 0x7dU
#define NOSTOS_UART_RAW_MAX (1U + NOSTOS_WIRE_MAX + 2U)
#define NOSTOS_UART_FRAME_MAX (2U * NOSTOS_UART_RAW_MAX + 2U)
#define NOSTOS_UART_TIMEOUT_MS 100U
typedef struct {
    uint8_t raw[NOSTOS_UART_RAW_MAX];
    size_t used;
    uint32_t last_byte_ms;
    bool active, escaped, dropping;
} nostos_uart_parser_t;
uint16_t nostos_crc16(const uint8_t *bytes, size_t length);
nostos_result_t nostos_uart_encode(const uint8_t *wire, size_t length,
    uint8_t *frame, size_t capacity, size_t *frame_length);
void nostos_uart_reset(nostos_uart_parser_t *parser);
/* One input byte; OK only on a complete CRC-checked frame. Caller owns outputs.
 * EMPTY means more input. timeout/overflow/error drops through next flag. */
nostos_result_t nostos_uart_feed(nostos_uart_parser_t *parser, uint8_t byte,
    uint32_t now_ms, uint8_t wire[NOSTOS_WIRE_MAX], size_t *length);
#endif
