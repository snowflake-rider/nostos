/*
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 */
/* 김현수의 collect_input 기반. originals/day33-v2.md 참조.
 * STMicroelectronics copyright/license notice is preserved in that original.
 * HAL callbacks and board setup are not installed by this host module.
 */
#include "uart_chat.h"
#include <stdio.h>
#include <string.h>

bool rp33_chat_init(rp33_Chat *chat, rp33_Write send, rp33_Write echo, void *context)
{
    if (chat == NULL || send == NULL || echo == NULL) return false;
    *chat = (rp33_Chat){.send = send, .echo = echo, .context = context};
    return true;
}

bool rp33_chat_feed(rp33_Chat *chat, const uint8_t *data, size_t size)
{
    static const uint8_t newline[] = "\r\n";
    static const uint8_t erase[] = "\b \b";
    if (chat == NULL || (data == NULL && size != 0) ||
        chat->send == NULL || chat->echo == NULL ||
        chat->length >= RP33_BUFFER_SIZE) return false;
    for (size_t i = 0; i < size; ++i) {
        const uint8_t ch = data[i];
        if (ch == '\r' || ch == '\n') {
            if (chat->length == 0) continue;
            chat->buffer[chat->length] = '\0';
            chat->send(chat->context, chat->buffer, chat->length);
            chat->send(chat->context, newline, 2);
            chat->echo(chat->context, newline, 2);
            memset(chat->buffer, 0, sizeof chat->buffer);
            chat->length = 0;
        } else if (ch == '\b' || ch == 0x7f) {
            if (chat->length != 0) {
                chat->echo(chat->context, erase, 3);
                /* Decrement before clearing the removed character. */
                chat->buffer[--chat->length] = '\0';
            }
        } else if (chat->length < RP33_BUFFER_SIZE - 1) {
            chat->buffer[chat->length++] = ch;
            chat->echo(chat->context, &ch, 1);
        }
    }
    return true;
}
static void show_sent(void *context, const uint8_t *data, size_t size)
{
    (void)context;
    (void)fwrite(data, 1, size, stdout);
}
static void ignore_echo(void *context, const uint8_t *data, size_t size)
{
    (void)context; (void)data; (void)size;
}
int rp33_demo(void)
{
    rp33_Chat chat;
    const uint8_t input[] = "hellp\bo\r\n";
    puts("=== Day 33: UART Chat (host input logic only) ===");
    if (!rp33_chat_init(&chat, show_sent, ignore_echo, NULL)) return 1;
    return rp33_chat_feed(&chat, input, sizeof input - 1) ? 0 : 1;
}
