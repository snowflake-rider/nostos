/* 김현수, Redpill Day 8. See README.md for contracts and adaptations. */
#ifndef REDPILL_MEMMOVE_H
#define REDPILL_MEMMOVE_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

uint8_t *rp08_my_memmove(uint8_t *dest, const uint8_t *src, size_t byte_count);

int rp08_demo(void);

#endif
