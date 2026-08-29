/* 김현수, Redpill Day 23. See README.md for contracts and adaptations. */
#ifndef REDPILL_TOKENIZER_H
#define REDPILL_TOKENIZER_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

typedef struct
{
    const char *source;
    const char delimiter;
} rp23_Tokenizer;

typedef struct
{
    const char *start;
    size_t length;
} rp23_Token;

/* Non-NULL starts/restarts; NULL continues the single shared static cursor.
 * Source must outlive the scan. Delimiter must be nonzero. Not reentrant. */
bool rp23_next_token(const rp23_Tokenizer *tokenizer, rp23_Token *token);

int rp23_demo(void);

#endif
