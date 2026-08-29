# Day 17 — 김현수 원문

출처: https://app.notion.com/2e02ae700871820a836d818cd6917154

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define BITS_PER_BYTE 8U
#define NUM_RESOURCES 20u
#define SAMPLE_NUM_RESOURCES 8u
// allocate enough bytes for capacity (num_resources)
#define BITMAP_BYTES(n) (((n) + BITS_PER_BYTE - 1U) / BITS_PER_BYTE) // ceil
// check if bin(var)[pos] == 0
#define BIT_IS_CLEAR(var, pos) (((var) & (1U << (pos))) == 0)
// BIT_CLEAR: bin(var)[pos] = 0
#define BIT_CLEAR(var, pos) ((var) &= ~(1U << (pos)))
// BIT_SET: bin(var)[pos] == 1
#define BIT_SET(var, pos) ((var) |= (1U << (pos)))
// BIT_GET: return bin(var)[pos]
#define BIT_GET(var, pos) (((var) >> (pos)) & 1U)

typedef struct Bitmap
{
  size_t capacity;
  uint8_t resources[BITMAP_BYTES(NUM_RESOURCES)];
} Bitmap;

// Initialize an empty bitmap with a capacity up to NUM_RESOURCES.
static bool bitmap_init(Bitmap *bm, size_t capacity)
{
  if (bm == NULL || capacity > NUM_RESOURCES)
  {
    return false;
  }

  bm->capacity = capacity;
  memset(bm->resources, 0, sizeof(bm->resources));
  return true;
}

// Allocate the lowest free resource index; return false if full or invalid.
static bool bitmap_alloc(Bitmap *bm, size_t *allocated_idx)
{
  // Reject when bm is NULL or allocated_idx is NULL
  if (bm == NULL || allocated_idx == NULL)
  {
    printf(">> Failed to allocate. Check memory for both bm and allocated_idx.\n");
    return false;
  }
  // number of bytes in bm
  size_t byte_count = BITMAP_BYTES(bm->capacity);

  for (size_t byte_idx = 0; byte_idx < byte_count; ++byte_idx)
  {
    // the byte is 0xFF -> resource fully allocated. move on to next resource available.
    if (bm->resources[byte_idx] == 0xFFU)
    {
      continue;
    }
    // found where this bitmap manager can allocate resource
    for (size_t bit_idx = 0; bit_idx < BITS_PER_BYTE; ++bit_idx)
    {
      size_t idx = byte_idx * BITS_PER_BYTE + bit_idx;
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
static bool bitmap_free(Bitmap *bm, size_t idx)
{
  if (bm == NULL)
  {
    return false;
  }

  size_t byte_idx = idx / BITS_PER_BYTE;
  size_t bit_idx = idx % BITS_PER_BYTE;
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
static void print_bitmap_visual(const Bitmap *bm)
{
  if (bm == NULL)
  {
    return;
  }

  printf("[Bitmap Visual] ");
  for (size_t idx = 0; idx < bm->capacity; ++idx)
  {
    if (idx > 0 && idx % BITS_PER_BYTE == 0)
    {
      printf(" | ");
    }

    size_t byte_idx = idx / BITS_PER_BYTE;
    size_t bit_idx = idx % BITS_PER_BYTE;
    printf("%u", BIT_GET(bm->resources[byte_idx], bit_idx));
  }
  putchar('\n');
}

int main(void)
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
  Bitmap bm;

  printf("=== Day 17: Bitmap Manager ===\n\n");
  if (!bitmap_init(&bm, NUM_RESOURCES))
  {
    fprintf(stderr, "Failed to initialize bitmap.\n");
    return 1;
  }

  size_t allocated_idx = 0;
  for (size_t idx = 0; idx < SAMPLE_NUM_RESOURCES; ++idx)
  {
    if (bitmap_alloc(&bm, &allocated_idx))
    {
      size_t byte_idx = allocated_idx / BITS_PER_BYTE;
      size_t bit_idx = allocated_idx % BITS_PER_BYTE;
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
  if (bitmap_free(&bm, target_idx))
  {
    size_t byte_idx = target_idx / BITS_PER_BYTE;
    size_t bit_idx = target_idx % BITS_PER_BYTE;
    printf(">> Index %zu freed. (Byte %zu, Bit %zu cleared)\n",
           target_idx, byte_idx, bit_idx);
  }
  print_bitmap_visual(&bm);
  putchar('\n');
  // Allocate (expected index -> 2)
  printf("Allocating again (expecting index %zu)...\n", target_idx);

  if (bitmap_alloc(&bm, &allocated_idx))
  {
    size_t byte_idx = allocated_idx / BITS_PER_BYTE;
    size_t bit_idx = allocated_idx % BITS_PER_BYTE;
    printf(">> Index %zu allocated. (Byte %zu, Bit %zu set)\n",
           allocated_idx, byte_idx, bit_idx);
  }

  print_bitmap_visual(&bm);

  return 0;
}


````
