/* 김현수, Redpill Day 13. See README.md for contracts and adaptations. */
#ifndef REDPILL_POOL_H
#define REDPILL_POOL_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

#define rp13_BLOCK_SIZE 32

#define rp13_POOL_SIZE 10

typedef struct rp13_Block
{
    struct rp13_Block *next;
} rp13_Block;

typedef struct
{
    // 실제 메모리 공간 (바이트 배열)
    // 정렬(Alignment)을 위해 uint64_t나 align 속성을 사용할 수 있음
    _Alignas(max_align_t) uint8_t memory_area[rp13_POOL_SIZE * rp13_BLOCK_SIZE];

    // 현재 사용 가능한 첫 번째 블록을 가리키는 포인터
    uint8_t *free_list;
    bool allocated[rp13_POOL_SIZE];

    // 디버깅용: 현재 사용 중인 블록 수
    int used_count;
} rp13_MemoryPool;

void rp13_pool_init(rp13_MemoryPool *pool);

void *rp13_pool_alloc(rp13_MemoryPool *pool);

/* Byte storage: access allocated payload through memcpy or unsigned char.
 * Rejects foreign/interior pointers and double free. Not thread safe. */
bool rp13_pool_free(rp13_MemoryPool *pool, void *ptr);

int rp13_demo(void);

#endif
