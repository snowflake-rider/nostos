# Day 13 — 김현수 원문

출처: https://app.notion.com/c422ae700871822185840128977c534d

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stddef.h>

// 설정: 블록 크기와 개수
#define BLOCK_SIZE 32
#define POOL_SIZE 10

// 메모리 블록 구조체 (Free List 관리를 위한 연결 리스트 노드)
// 할당되지 않았을 때는 다음 빈 블록을 가리키는 포인터로 사용됨
typedef struct Block
{
    struct Block *next;
} Block;

// 메모리 풀 컨텍스트
typedef struct
{
    // 실제 메모리 공간 (바이트 배열)
    // 정렬(Alignment)을 위해 uint64_t나 align 속성을 사용할 수 있음
    uint8_t memory_area[POOL_SIZE * BLOCK_SIZE];

    // 현재 사용 가능한 첫 번째 블록을 가리키는 포인터
    Block *free_list;

    // 디버깅용: 현재 사용 중인 블록 수
    int used_count;
} MemoryPool;

// Memory Pool
static MemoryPool my_pool = {
    .memory_area = {0}, .free_list = NULL, .used_count = 0};

// 1. 초기화: 모든 메모리를 쪼개서 연결 리스트로 연결
void pool_init(MemoryPool *pool);

// 2. 할당: Free List의 헤드(Head)를 떼어줌 (Pop)
void *pool_alloc(MemoryPool *pool);

// 3. 해제: 반환된 블록을 Free List의 헤드에 다시 붙임 (Push)
void pool_free(MemoryPool *pool, void *ptr);

// 테스트 코드
int main(void)
{
    pool_init(&my_pool);

    // 3개 할당
    void *p1 = pool_alloc(&my_pool);
    void *p2 = pool_alloc(&my_pool);
    void *p3 = pool_alloc(&my_pool);

    printf("Allocated: %p, %p, %p\n", p1, p2, p3);
    printf("Used Blocks: %d\n", my_pool.used_count);

    // 1개 해제 (p2)
    printf("Freeing %p...\n", p2);
    pool_free(&my_pool, p2); // 이제 p2가 free_list의 head가 됨

    // 다시 1개 할당 (p2와 같은 주소가 나와야 함 - LIFO 특성)
    void *p4 = pool_alloc(&my_pool);
    printf("Re-allocated: %p (Should be same as old p2)\n", p4);

    return 0;
}

// 1. 초기화: 모든 메모리를 쪼개서 연결 리스트로 연결
void pool_init(MemoryPool *pool)
{

    assert(pool != NULL);
    // 인접한 블록들을 연결하여 초기 Free List를 구성
    for (size_t i = 0; i < POOL_SIZE - 1; ++i)
    {
        // 1. 메모리 영역의 i번째 BLOCK_SIZE 바이트 공간을 Free List 노드로 해석
        Block *curr_block = (Block *)(pool->memory_area + i * BLOCK_SIZE);
        // 2. 현재 블록이 다음 블록을 가리키도록 연결
        curr_block->next = (Block *)(pool->memory_area + (i + 1) * BLOCK_SIZE);
    }
    // 마지막 블록 --> NULL
    Block *last = (Block *)(pool->memory_area + (POOL_SIZE - 1) * BLOCK_SIZE);
    last->next = NULL;
    // Free List의 헤드(Head) --> 첫 번째 블록
    pool->free_list = (Block *)pool->memory_area;
    pool->used_count = 0;
    // 메모리 풀 초기화 완료 메시지
    printf("[Init] Memory Pool Initialized (%d blocks of %d bytes)\n", POOL_SIZE, BLOCK_SIZE);
}

// 2. 할당: Free List의 헤드(Head)를 떼어줌 (Pop)
void *pool_alloc(MemoryPool *pool)
{

    assert(pool != NULL);
    // 사용 가능한 블록이 없으면 할당 실패
    if (pool->free_list == NULL)
    {
        fprintf(stderr, "No Free Block Available.\n");
        return NULL;
    }

    Block *curr_block = pool->free_list;
    // Free List의 헤드(Head) --> 다음 사용 가능한 블록
    pool->free_list = curr_block->next;
    pool->used_count += 1;
    return curr_block;
}

// 3. 해제: 반환된 블록을 Free List의 헤드에 다시 붙임 (Push)
void pool_free(MemoryPool *pool, void *ptr)
{
    assert(pool != NULL);
    assert(ptr != NULL);
    assert(pool->used_count > 0);
    Block *returned_block = (Block *)ptr;
    returned_block->next = pool->free_list;
    // Free List의 헤드(Head) --> 반환된 블록
    pool->free_list = returned_block;
    pool->used_count -= 1;
}

````
