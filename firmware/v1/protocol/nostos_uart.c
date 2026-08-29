#include "nostos_uart.h"
#include <string.h>
uint16_t nostos_crc16(const uint8_t *b, size_t n)
{
    uint16_t crc=0xffff;
    for (size_t i=0; i<n; ++i) {
        crc^=(uint16_t)((uint16_t)b[i]<<8);
        for (unsigned j=0; j<8; ++j)
            crc=(uint16_t)((uint16_t)(crc<<1)^((crc&0x8000U)?0x1021U:0U));
    }
    return crc;
}
nostos_result_t nostos_uart_encode(const uint8_t *w, size_t n, uint8_t *out, size_t cap, size_t *len)
{
    if (!w || !out || !len) return NOSTOS_BAD_ARGUMENT;
    if (n>NOSTOS_WIRE_MAX) return NOSTOS_TOO_LARGE;
    if (n<NOSTOS_HEADER_SIZE) return NOSTOS_BAD_LENGTH;
    uint8_t raw[NOSTOS_UART_RAW_MAX], frame[NOSTOS_UART_FRAME_MAX];
    raw[0]=(uint8_t)n; memcpy(raw+1,w,n);
    uint16_t crc=nostos_crc16(raw,n+1);
    raw[n+1]=(uint8_t)crc; raw[n+2]=(uint8_t)(crc>>8);
    size_t used=0;
    frame[used++]=NOSTOS_UART_FLAG;
    for (size_t i=0; i<n+3; ++i) {
        if (raw[i]==NOSTOS_UART_FLAG || raw[i]==NOSTOS_UART_ESCAPE) {
            frame[used++]=NOSTOS_UART_ESCAPE; frame[used++]=(uint8_t)(raw[i]^0x20U);
        } else frame[used++]=raw[i];
    }
    frame[used++]=NOSTOS_UART_FLAG;
    if (cap<used) return NOSTOS_BAD_LENGTH;
    memcpy(out,frame,used); *len=used; return NOSTOS_OK;
}
void nostos_uart_reset(nostos_uart_parser_t *p) { if (p) *p=(nostos_uart_parser_t){0}; }
nostos_result_t nostos_uart_feed(nostos_uart_parser_t *p, uint8_t b, uint32_t now,
    uint8_t out[NOSTOS_WIRE_MAX], size_t *len)
{
    if (!p || !out || !len) return NOSTOS_BAD_ARGUMENT;
    bool timed_out=p->active && (p->used || p->escaped) &&
        (uint32_t)(now-p->last_byte_ms)>NOSTOS_UART_TIMEOUT_MS;
    if (timed_out) { nostos_uart_reset(p); }
    if (b==NOSTOS_UART_FLAG) {
        nostos_result_t r=timed_out?NOSTOS_TIMEOUT:NOSTOS_EMPTY;
        if (p->active && !p->dropping && p->used) {
            r=NOSTOS_BAD_LENGTH;
            size_t n=p->raw[0];
            if (!p->escaped && n>=NOSTOS_HEADER_SIZE && n<=NOSTOS_WIRE_MAX && p->used==n+3) {
                uint16_t got=(uint16_t)((uint16_t)p->raw[n+1] | (uint16_t)((uint16_t)p->raw[n+2]<<8));
                r=got==nostos_crc16(p->raw,n+1)?NOSTOS_OK:NOSTOS_BAD_CRC;
                if (r==NOSTOS_OK) { memcpy(out,p->raw+1,n); *len=n; }
            }
        }
        nostos_uart_reset(p); p->active=true; p->last_byte_ms=now; return r;
    }
    if (timed_out) return NOSTOS_TIMEOUT;
    if (!p->active || p->dropping) return NOSTOS_EMPTY;
    p->last_byte_ms=now;
    if (p->escaped) {
        p->escaped=false;
        if (b!=(NOSTOS_UART_FLAG^0x20U) && b!=(NOSTOS_UART_ESCAPE^0x20U)) {
            p->dropping=true; return NOSTOS_BAD_VALUE;
        }
        b^=0x20U;
    } else if (b==NOSTOS_UART_ESCAPE) { p->escaped=true; return NOSTOS_EMPTY; }
    if (p->used==NOSTOS_UART_RAW_MAX) { p->dropping=true; return NOSTOS_TOO_LARGE; }
    p->raw[p->used++]=b;
    if (p->used==1 && (b<NOSTOS_HEADER_SIZE || b>NOSTOS_WIRE_MAX)) {
        p->dropping=true; return b>NOSTOS_WIRE_MAX?NOSTOS_TOO_LARGE:NOSTOS_BAD_LENGTH;
    }
    return NOSTOS_EMPTY;
}
