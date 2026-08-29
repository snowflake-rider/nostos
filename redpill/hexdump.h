/* 김현수, Redpill Day 14. See README.md for contracts and adaptations. */
#ifndef REDPILL_HEXDUMP_H
#define REDPILL_HEXDUMP_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

void rp14_hexdump(const char *data_type, const void *data, const size_t size);

int rp14_demo(void);

#endif
