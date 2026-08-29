/* 김현수, Redpill Day 5. See README.md for contracts and adaptations. */
#ifndef REDPILL_POPCOUNT_H
#define REDPILL_POPCOUNT_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

unsigned int rp05_popcount_naive(uint32_t n);

unsigned int rp05_popcount_swar(uint32_t n);

unsigned int rp05_popcount_brian(uint32_t n);

unsigned int rp05_popcount_builtin(uint32_t n);

int rp05_demo(void);

#endif
