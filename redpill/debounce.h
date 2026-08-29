/* 김현수, Redpill Day 24. See README.md for contracts and adaptations. */
#ifndef REDPILL_DEBOUNCE_H
#define REDPILL_DEBOUNCE_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

typedef struct rp24_Debouncer
{
    bool stable_output;
    unsigned int counter;
    unsigned int threshold;
} rp24_Debouncer;

bool rp24_debounce(rp24_Debouncer *debouncer, bool raw_input);

bool rp24_init_debouncer(rp24_Debouncer *debouncer, const unsigned int threshold);

int rp24_demo(void);

#endif
