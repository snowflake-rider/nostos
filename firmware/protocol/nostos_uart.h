#ifndef NOSTOS_UART_H
#define NOSTOS_UART_H

#include "nostos_protocol.h"

#define NOSTOS_UART_MAGIC_0 0xA5U
#define NOSTOS_UART_MAGIC_1 0x5AU
#define NOSTOS_UART_TIMEOUT_MS 100U
#define NOSTOS_UART_CRC_SIZE 2U
#define NOSTOS_UART_FRAME_MAX \
    (2U + 1U + NOSTOS_APPLICATION_MAX_SIZE + NOSTOS_UART_CRC_SIZE)

typedef struct {
    uint8_t bytes[1U + NOSTOS_APPLICATION_MAX_SIZE];
    size_t used;
    size_t expected;
    uint32_t last_byte_ms;
    uint8_t state;
    uint8_t crc_low;
} nostos_uart_parser_t;

uint16_t nostos_crc16(const uint8_t *bytes, size_t length);

/* Normal path requires source_node_id 1..10.  Local path additionally permits
 * source 0 for STM32->paired ESP32 only. */
nostos_result_t nostos_uart_encode_message(const nostos_message_t *message,
    uint8_t *frame, size_t capacity, size_t *frame_length);
nostos_result_t nostos_uart_encode_local_message(const nostos_message_t *message,
    uint8_t *frame, size_t capacity, size_t *frame_length);

void nostos_uart_reset(nostos_uart_parser_t *parser);

/* One byte at a time.  EMPTY means that a complete frame is not available yet.
 * Outputs are unchanged on framing, CRC, or application validation errors. */
nostos_result_t nostos_uart_feed_message(nostos_uart_parser_t *parser,
    uint8_t byte, uint32_t now_ms, nostos_message_t *message);
nostos_result_t nostos_uart_feed_local_message(nostos_uart_parser_t *parser,
    uint8_t byte, uint32_t now_ms, nostos_message_t *message);

#endif
