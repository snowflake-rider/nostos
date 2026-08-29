/* 김현수 Day 13: 10 x 32 byte free-list pool.
 * Original: originals/day13.md. Added alignment and ownership validation.
 * Links use memcpy so a byte array is not dereferenced as an unrelated struct.
 */
#include "pool.h"
#include <stdio.h>
#include <string.h>
_Static_assert(rp13_BLOCK_SIZE >= sizeof(uint8_t *), "Block must hold a link");
_Static_assert(rp13_BLOCK_SIZE % _Alignof(max_align_t) == 0, "Block alignment");

void rp13_pool_init(rp13_MemoryPool *pool)
{
    if (pool == NULL) return;
    memset(pool->allocated, 0, sizeof pool->allocated);
    for (size_t i = 0; i < rp13_POOL_SIZE; ++i) {
        uint8_t *next = i + 1 < rp13_POOL_SIZE
                      ? pool->memory_area + (i + 1) * rp13_BLOCK_SIZE : NULL;
        memcpy(pool->memory_area + i * rp13_BLOCK_SIZE, &next, sizeof next);
    }
    pool->free_list = pool->memory_area;
    pool->used_count = 0;
}

void *rp13_pool_alloc(rp13_MemoryPool *pool)
{
    if (pool == NULL || pool->free_list == NULL) return NULL;
    uint8_t *block = pool->free_list;
    memcpy(&pool->free_list, block, sizeof pool->free_list);
    size_t index = (size_t)(block - pool->memory_area) / rp13_BLOCK_SIZE;
    pool->allocated[index] = true;
    ++pool->used_count;
    return block;
}

bool rp13_pool_free(rp13_MemoryPool *pool, void *ptr)
{
    if (pool == NULL || ptr == NULL) return false;
    for (size_t i = 0; i < rp13_POOL_SIZE; ++i) {
        uint8_t *block = pool->memory_area + i * rp13_BLOCK_SIZE;
        if (ptr != block) continue;
        if (!pool->allocated[i]) return false;
        memcpy(block, &pool->free_list, sizeof pool->free_list);
        pool->free_list = block;
        pool->allocated[i] = false;
        --pool->used_count;
        return true;
    }
    return false;
}

int rp13_demo(void)
{
    rp13_MemoryPool pool;
    rp13_pool_init(&pool);
    printf("[Init] Memory Pool Initialized (%d blocks of %d bytes)\n",
           rp13_POOL_SIZE, rp13_BLOCK_SIZE);
    void *p1 = rp13_pool_alloc(&pool);
    void *p2 = rp13_pool_alloc(&pool);
    void *p3 = rp13_pool_alloc(&pool);
    printf("Allocated: %p, %p, %p\nUsed Blocks: %d\n", p1, p2, p3, pool.used_count);
    if (!rp13_pool_free(&pool, p2)) return 1;
    void *p4 = rp13_pool_alloc(&pool);
    printf("Re-allocated: %p (Should be same as old p2)\n", p4);
    return p4 == p2 ? 0 : 1;
}
