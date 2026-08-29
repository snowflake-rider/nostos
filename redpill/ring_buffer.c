/* 김현수 제출 코드 기반. 원문: originals/day15.md */
#include "ring_buffer.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>



// RingBuffer: 구조체
// - head: 다음 데이터를 저장할 인덱스
// - tail: 다음 데이터를 읽을 인덱스
// - 인덱스 범위: 0 이상 BUFFER_SIZE 미만
// - Empty: head == tail
// - Full: (head + 1) % BUFFER_SIZE == tail


// rb_init: RingBuffer 초기화
void rp15_rb_init(rp15_RingBuffer *rb);
// rb_put: Full이면 변경 없이 false, 저장에 성공하면 true
bool rp15_rb_put(rp15_RingBuffer *rb, uint8_t data);
// rb_gett: Empty이면 변경 없이 false, 읽기에 성공하면 data에 저장하고 true
bool rp15_rb_get(rp15_RingBuffer *rb, uint8_t *data);

// ---- 테스트 코드 ----
int rp15_demo(void)
{
    rp15_RingBuffer uart_rx_buf;
    uint8_t temp;

    rp15_rb_init(&uart_rx_buf);

    printf("--- Phase 1: Filling Buffer ---\n");
    // 버퍼 사이즈가 8이므로, 한 칸 비우면 최대 7개 저장 가능
    // 🟥 NOTE: changed i's data type from `int` to `uint8_t`
    for (uint8_t i = 1; i <= 8; i++)
    {
        if (rp15_rb_put(&uart_rx_buf, i))
        {
            printf("Put: %d (Head: %u, Tail: %u)\n", i, uart_rx_buf.head, uart_rx_buf.tail);
        }
        else
        {
            printf("Fail to Put: %d (Buffer Full!)\n", i);
        }
    }

    printf("\n--- Phase 2: Reading Buffer ---\n");
    // 3개만 읽어봄
    for (uint8_t i = 0; i < 3; i++)
    {
        if (rp15_rb_get(&uart_rx_buf, &temp))
        {
            printf("Get: %d (Head: %u, Tail: %u)\n", temp, uart_rx_buf.head, uart_rx_buf.tail);
        }
    }

    printf("\n--- Phase 3: Writing again (Wrap around) ---\n");
    // 읽어서 공간이 생겼으므로 다시 쓰기 가능 (인덱스가 0으로 돌아가는지 확인)
    if (rp15_rb_put(&uart_rx_buf, 99))
    {
        printf("Put: 99 (Head: %u, Tail: %u)\n", uart_rx_buf.head, uart_rx_buf.tail);
    }

    return 0;
}

void rp15_rb_init(rp15_RingBuffer *rb)
{
    assert(rb != NULL);
    rb->head = 0;
    rb->tail = 0;
}

bool rp15_rb_put(rp15_RingBuffer *rb, uint8_t data)
{
    assert(rb != NULL);
    if (((rb->head + 1) % rp15_BUFFER_SIZE) == rb->tail)
    {
        return false;
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rp15_BUFFER_SIZE;
    return true;
}

bool rp15_rb_get(rp15_RingBuffer *rb, uint8_t *data)
{
    assert(rb != NULL);
    assert(data != NULL);
    if (rb->head == rb->tail)
    {
        return false;
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rp15_BUFFER_SIZE;
    return true;
}
