/* 김현수 Day 33 "김현수 2" collect_input에서 HAL 의존성을 분리했다. */
#ifndef REDPILL_UART_CHAT_H
#define REDPILL_UART_CHAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RP33_BUFFER_SIZE 64
typedef void (*rp33_Write)(void *context, const uint8_t *data, size_t size);
typedef struct {
    uint8_t buffer[RP33_BUFFER_SIZE];
    size_t length;
    rp33_Write send;
    rp33_Write echo;
    void *context;
} rp33_Chat;
/* Synchronous callbacks must consume/copy data before returning; no reentry.
 * Initialization requires both callbacks. Call feed only after successful init.
 * Full buffer ignores excess bytes, matching the class code.
 */
bool rp33_chat_init(rp33_Chat *chat, rp33_Write send, rp33_Write echo, void *context);
bool rp33_chat_feed(rp33_Chat *chat, const uint8_t *data, size_t size);
int rp33_demo(void);
#endif
