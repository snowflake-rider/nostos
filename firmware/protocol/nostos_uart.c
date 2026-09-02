#include "nostos_uart.h"

#include <string.h>

enum {
    UART_WAIT_MAGIC_0 = 0,
    UART_WAIT_MAGIC_1,
    UART_READ_LENGTH,
    UART_READ_MESSAGE,
    UART_READ_CRC_LOW,
    UART_READ_CRC_HIGH
};

uint16_t nostos_crc16(const uint8_t *bytes, size_t length)
{
    if (!bytes && length != 0U) return 0U;
    uint16_t crc = 0xFFFFU;
    for (size_t index = 0U; index < length; ++index) {
        crc ^= (uint16_t)((uint16_t)bytes[index] << 8U);
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (uint16_t)((uint16_t)(crc << 1U) ^
                ((crc & 0x8000U) != 0U ? 0x1021U : 0U));
        }
    }
    return crc;
}

static nostos_result_t uart_encode_wire(const uint8_t *wire, size_t length,
    uint8_t *frame, size_t capacity, size_t *frame_length)
{
    if (!wire || !frame || !frame_length) return NOSTOS_BAD_ARGUMENT;
    if (length > NOSTOS_APPLICATION_MAX_SIZE) return NOSTOS_TOO_LARGE;
    if (length < NOSTOS_APPLICATION_MIN_SIZE) return NOSTOS_BAD_LENGTH;
    size_t required = 2U + 1U + length + NOSTOS_UART_CRC_SIZE;
    if (capacity < required) return NOSTOS_BAD_LENGTH;

    uint8_t encoded[NOSTOS_UART_FRAME_MAX] = {0};
    encoded[0] = NOSTOS_UART_MAGIC_0;
    encoded[1] = NOSTOS_UART_MAGIC_1;
    encoded[2] = (uint8_t)length;
    memcpy(encoded + 3, wire, length);
    uint16_t crc = nostos_crc16(encoded + 2, length + 1U);
    encoded[3U + length] = (uint8_t)crc;
    encoded[4U + length] = (uint8_t)(crc >> 8U);
    memcpy(frame, encoded, required);
    *frame_length = required;
    return NOSTOS_OK;
}

static nostos_result_t encode_message_policy(const nostos_message_t *message,
    bool local, uint8_t *frame, size_t capacity, size_t *frame_length)
{
    if (!message || !frame || !frame_length) return NOSTOS_BAD_ARGUMENT;
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    size_t wire_length = 0U;
    nostos_result_t result = local ?
        nostos_local_message_encode(message, wire, sizeof(wire), &wire_length) :
        nostos_message_encode(message, wire, sizeof(wire), &wire_length);
    if (result != NOSTOS_OK) return result;
    return uart_encode_wire(wire, wire_length, frame, capacity, frame_length);
}

nostos_result_t nostos_uart_encode_message(const nostos_message_t *message,
    uint8_t *frame, size_t capacity, size_t *frame_length)
{
    return encode_message_policy(message, false, frame, capacity, frame_length);
}

nostos_result_t nostos_uart_encode_local_message(const nostos_message_t *message,
    uint8_t *frame, size_t capacity, size_t *frame_length)
{
    return encode_message_policy(message, true, frame, capacity, frame_length);
}

void nostos_uart_reset(nostos_uart_parser_t *parser)
{
    if (parser) *parser = (nostos_uart_parser_t){0};
}

static nostos_result_t process_byte(nostos_uart_parser_t *parser, uint8_t byte,
    uint32_t now_ms, uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE], size_t *length)
{
    switch (parser->state) {
    case UART_WAIT_MAGIC_0:
        if (byte == NOSTOS_UART_MAGIC_0) {
            parser->state = UART_WAIT_MAGIC_1;
            parser->last_byte_ms = now_ms;
        }
        return NOSTOS_EMPTY;
    case UART_WAIT_MAGIC_1:
        parser->last_byte_ms = now_ms;
        if (byte == NOSTOS_UART_MAGIC_1) {
            parser->state = UART_READ_LENGTH;
        } else if (byte != NOSTOS_UART_MAGIC_0) {
            parser->state = UART_WAIT_MAGIC_0;
        }
        return NOSTOS_EMPTY;
    case UART_READ_LENGTH:
        parser->last_byte_ms = now_ms;
        if (byte > NOSTOS_APPLICATION_MAX_SIZE) {
            nostos_uart_reset(parser);
            if (byte == NOSTOS_UART_MAGIC_0) {
                parser->state = UART_WAIT_MAGIC_1;
                parser->last_byte_ms = now_ms;
            }
            return NOSTOS_TOO_LARGE;
        }
        if (byte < NOSTOS_APPLICATION_MIN_SIZE) {
            nostos_uart_reset(parser);
            if (byte == NOSTOS_UART_MAGIC_0) {
                parser->state = UART_WAIT_MAGIC_1;
                parser->last_byte_ms = now_ms;
            }
            return NOSTOS_BAD_LENGTH;
        }
        parser->bytes[0] = byte;
        parser->used = 1U;
        parser->expected = (size_t)byte;
        parser->state = UART_READ_MESSAGE;
        return NOSTOS_EMPTY;
    case UART_READ_MESSAGE:
        parser->last_byte_ms = now_ms;
        parser->bytes[parser->used++] = byte;
        if (parser->used == parser->expected + 1U) {
            parser->state = UART_READ_CRC_LOW;
        }
        return NOSTOS_EMPTY;
    case UART_READ_CRC_LOW:
        parser->last_byte_ms = now_ms;
        parser->crc_low = byte;
        parser->state = UART_READ_CRC_HIGH;
        return NOSTOS_EMPTY;
    case UART_READ_CRC_HIGH: {
        size_t message_length = parser->expected;
        uint16_t received = (uint16_t)((uint16_t)parser->crc_low |
            (uint16_t)((uint16_t)byte << 8U));
        uint16_t expected = nostos_crc16(parser->bytes, message_length + 1U);
        if (received != expected) {
            nostos_uart_reset(parser);
            return NOSTOS_BAD_CRC;
        }
        uint8_t decoded[NOSTOS_APPLICATION_MAX_SIZE];
        memcpy(decoded, parser->bytes + 1, message_length);
        nostos_uart_reset(parser);
        memcpy(wire, decoded, message_length);
        *length = message_length;
        return NOSTOS_OK;
    }
    default:
        nostos_uart_reset(parser);
        return NOSTOS_BAD_VALUE;
    }
}

static nostos_result_t uart_feed_wire(nostos_uart_parser_t *parser, uint8_t byte,
    uint32_t now_ms, uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE], size_t *length)
{
    if (!parser || !wire || !length) return NOSTOS_BAD_ARGUMENT;
    bool timed_out = parser->state != UART_WAIT_MAGIC_0 &&
        (uint32_t)(now_ms - parser->last_byte_ms) > NOSTOS_UART_TIMEOUT_MS;
    if (timed_out) {
        nostos_uart_reset(parser);
        (void)process_byte(parser, byte, now_ms, wire, length);
        return NOSTOS_TIMEOUT;
    }
    return process_byte(parser, byte, now_ms, wire, length);
}

static nostos_result_t feed_message_policy(nostos_uart_parser_t *parser,
    uint8_t byte, uint32_t now_ms, bool local, nostos_message_t *message)
{
    if (!parser || !message) return NOSTOS_BAD_ARGUMENT;
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    size_t length = 0U;
    nostos_result_t result = uart_feed_wire(parser, byte, now_ms, wire, &length);
    if (result != NOSTOS_OK) return result;
    return local ? nostos_local_message_decode(wire, length, message) :
        nostos_message_decode(wire, length, message);
}

nostos_result_t nostos_uart_feed_message(nostos_uart_parser_t *parser,
    uint8_t byte, uint32_t now_ms, nostos_message_t *message)
{
    return feed_message_policy(parser, byte, now_ms, false, message);
}

nostos_result_t nostos_uart_feed_local_message(nostos_uart_parser_t *parser,
    uint8_t byte, uint32_t now_ms, nostos_message_t *message)
{
    return feed_message_policy(parser, byte, now_ms, true, message);
}
