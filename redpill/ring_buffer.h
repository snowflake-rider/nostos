/* 김현수, Redpill Day 15. See README.md for contracts and adaptations. */
#ifndef REDPILL_RING_BUFFER_H
#define REDPILL_RING_BUFFER_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

#define rp15_BUFFER_SIZE 8

typedef struct
{
    unsigned int head;
    unsigned int tail;
    uint8_t buffer[rp15_BUFFER_SIZE];
} rp15_RingBuffer;

void rp15_rb_init(rp15_RingBuffer *rb);

bool rp15_rb_put(rp15_RingBuffer *rb, uint8_t data);

bool rp15_rb_get(rp15_RingBuffer *rb, uint8_t *data);

int rp15_demo(void);

#endif
