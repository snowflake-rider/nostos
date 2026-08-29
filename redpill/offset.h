/* 김현수, Redpill Day 12. See README.md for contracts and adaptations. */
#ifndef REDPILL_OFFSET_H
#define REDPILL_OFFSET_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

/* Object-based alternative to the original null-pointer/type macro.
 * Pass a real struct object, not an expression with side effects. */
#define RP12_MEMBER_OFFSET(object, member) \
    ((size_t)((const unsigned char *)&(object).member - \
              (const unsigned char *)&(object)))

int rp12_demo(void);

#endif
