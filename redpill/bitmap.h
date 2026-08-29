/* 김현수, Redpill Day 17. See README.md for contracts and adaptations. */
#ifndef REDPILL_BITMAP_H
#define REDPILL_BITMAP_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

#define rp17_BITS_PER_BYTE 8U

#define rp17_NUM_RESOURCES 20u

#define rp17_BITMAP_BYTES(n) (((n) + rp17_BITS_PER_BYTE - 1U) / rp17_BITS_PER_BYTE) // ceil

typedef struct rp17_Bitmap
{
  size_t capacity;
  uint8_t resources[rp17_BITMAP_BYTES(rp17_NUM_RESOURCES)];
} rp17_Bitmap;

bool rp17_bitmap_init(rp17_Bitmap *bm, size_t capacity);

bool rp17_bitmap_alloc(rp17_Bitmap *bm, size_t *allocated_idx);

bool rp17_bitmap_free(rp17_Bitmap *bm, size_t idx);

int rp17_demo(void);

#endif
