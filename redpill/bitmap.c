/* 김현수 제출 코드 기반. 원문: originals/day17.md */
#include "bitmap.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>



#define SAMPLE_NUM_RESOURCES 8u
// allocate enough bytes for capacity (num_resources)

// check if bin(var)[pos] == 0
#define BIT_IS_CLEAR(var, pos) (((var) & (1U << (pos))) == 0)
// BIT_CLEAR: bin(var)[pos] = 0
#define BIT_CLEAR(var, pos) ((var) &= ~(1U << (pos)))
// BIT_SET: bin(var)[pos] == 1
#define BIT_SET(var, pos) ((var) |= (1U << (pos)))
// BIT_GET: return bin(var)[pos]
#define BIT_GET(var, pos) (((var) >> (pos)) & 1U)



// Initialize an empty bitmap with a capacity up to NUM_RESOURCES.
bool rp17_bitmap_init(rp17_Bitmap *bm, size_t capacity)
{
  if (bm == NULL || capacity > rp17_NUM_RESOURCES)
  {
    return false;
  }

  bm->capacity = capacity;
  memset(bm->resources, 0, sizeof(bm->resources));
  return true;
}

// Allocate the lowest free resource index; return false if full or invalid.
bool rp17_bitmap_alloc(rp17_Bitmap *bm, size_t *allocated_idx)
{
  // Reject when bm is NULL or allocated_idx is NULL
  if (bm == NULL || allocated_idx == NULL)
  {
    printf(">> Failed to allocate. Check memory for both bm and allocated_idx.\n");
    return false;
  }
  // number of bytes in bm
  size_t byte_count = rp17_BITMAP_BYTES(bm->capacity);

  for (size_t byte_idx = 0; byte_idx < byte_count; ++byte_idx)
  {
    // the byte is 0xFF -> resource fully allocated. move on to next resource available.
    if (bm->resources[byte_idx] == 0xFFU)
    {
      continue;
    }
    // found where this bitmap manager can allocate resource
    for (size_t bit_idx = 0; bit_idx < rp17_BITS_PER_BYTE; ++bit_idx)
    {
      size_t idx = byte_idx * rp17_BITS_PER_BYTE + bit_idx;
      // out of capacity
      if (idx >= bm->capacity)
      {
        return false;
      }

      if (BIT_IS_CLEAR(bm->resources[byte_idx], bit_idx))
      {
        BIT_SET(bm->resources[byte_idx], bit_idx);
        *allocated_idx = idx;
        return true;
      }
    }
  }
  return false;
}

// Release an allocated resource; reject invalid indices and double-free.
bool rp17_bitmap_free(rp17_Bitmap *bm, size_t idx)
{
  if (bm == NULL)
  {
    return false;
  }

  size_t byte_idx = idx / rp17_BITS_PER_BYTE;
  size_t bit_idx = idx % rp17_BITS_PER_BYTE;
  // Filter out wrong idx
  if (idx >= bm->capacity)
  {
    printf(">> Free failed. (index %zu out of range)\n", idx);
    return false;
  }
  // Prevent double free
  if (BIT_IS_CLEAR(bm->resources[byte_idx], bit_idx))
  {
    return false;
  }
  // free the resource in bitmap
  BIT_CLEAR(bm->resources[byte_idx], bit_idx);
  return true;
}

// Print logical resource bits in groups of eight.
static void print_bitmap_visual(const rp17_Bitmap *bm)
{
  if (bm == NULL)
  {
    return;
  }

  printf("[Bitmap Visual] ");
  for (size_t idx = 0; idx < bm->capacity; ++idx)
  {
    if (idx > 0 && idx % rp17_BITS_PER_BYTE == 0)
    {
      printf(" | ");
    }

    size_t byte_idx = idx / rp17_BITS_PER_BYTE;
    size_t bit_idx = idx % rp17_BITS_PER_BYTE;
    printf("%u", BIT_GET(bm->resources[byte_idx], bit_idx));
  }
  putchar('\n');
}

int rp17_demo(void)
{

  /**
 * === Day 17: Bitmap Manager ===

>> Index 0 allocated. (Byte 0, Bit 0 set)
>> Index 1 allocated. (Byte 0, Bit 1 set)
>> Index 2 allocated. (Byte 0, Bit 2 set)
>> Index 3 allocated. (Byte 0, Bit 3 set)
>> Index 4 allocated. (Byte 0, Bit 4 set)
>> Index 5 allocated. (Byte 0, Bit 5 set)
>> Index 6 allocated. (Byte 0, Bit 6 set)
>> Index 7 allocated. (Byte 0, Bit 7 set)
[Bitmap Visual] 11111111 | 00000000 | 0000

Freeing index 2...
>> Index 2 freed. (Byte 0, Bit 2 cleared)
[Bitmap Visual] 11011111 | 00000000 | 0000

Allocating again (expecting index 2)...
>> Index 2 allocated. (Byte 0, Bit 2 set)
[Bitmap Visual] 11111111 | 00000000 | 0000
 */
  rp17_Bitmap bm;

  printf("=== Day 17: Bitmap Manager ===\n\n");
  if (!rp17_bitmap_init(&bm, rp17_NUM_RESOURCES))
  {
    fprintf(stderr, "Failed to initialize bitmap.\n");
    return 1;
  }

  size_t allocated_idx = 0;
  for (size_t idx = 0; idx < SAMPLE_NUM_RESOURCES; ++idx)
  {
    if (rp17_bitmap_alloc(&bm, &allocated_idx))
    {
      size_t byte_idx = allocated_idx / rp17_BITS_PER_BYTE;
      size_t bit_idx = allocated_idx % rp17_BITS_PER_BYTE;
      printf(">> Index %zu allocated. (Byte %zu, Bit %zu set)\n",
             allocated_idx, byte_idx, bit_idx);
    }
  }
  // print Bitmap Visual
  print_bitmap_visual(&bm);
  putchar('\n');
  // Free index 2
  size_t target_idx = 2U;
  printf("Freeing index %zu...\n", target_idx);
  if (rp17_bitmap_free(&bm, target_idx))
  {
    size_t byte_idx = target_idx / rp17_BITS_PER_BYTE;
    size_t bit_idx = target_idx % rp17_BITS_PER_BYTE;
    printf(">> Index %zu freed. (Byte %zu, Bit %zu cleared)\n",
           target_idx, byte_idx, bit_idx);
  }
  print_bitmap_visual(&bm);
  putchar('\n');
  // Allocate (expected index -> 2)
  printf("Allocating again (expecting index %zu)...\n", target_idx);

  if (rp17_bitmap_alloc(&bm, &allocated_idx))
  {
    size_t byte_idx = allocated_idx / rp17_BITS_PER_BYTE;
    size_t bit_idx = allocated_idx % rp17_BITS_PER_BYTE;
    printf(">> Index %zu allocated. (Byte %zu, Bit %zu set)\n",
           allocated_idx, byte_idx, bit_idx);
  }

  print_bitmap_visual(&bm);

  return 0;
}
