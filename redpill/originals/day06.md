# Day 6 — 김현수 원문

출처: https://app.notion.com/b5c2ae70087182288a31017269b58985

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdint.h>
// #include <stdbool.h> 
/**
 * - **Day 6. 원형 비트 시프트 (Circular Shift/Rotate)**
    - **입력:** 32비트 정수, 이동할 비트 수 `n`, 방향(Left/Right)
    - **출력:** 회전된 결과값
    - **제약조건:** 버려지는 비트가 반대편으로 채워져야 함.
    - **실행결과:**

    ```c
    === Day 6: Circular Shift (Rotate) ===

    [Init]   Hex: 0xF0000000
             Bin: 1111 0000 0000 0000 0000 0000 0000 0000

    [ROL 4]  Hex: 0x0000000F
             Bin: 0000 0000 0000 0000 0000 0000 0000 1111
             (MSB bits moved to LSB)

    [ROR 4]  Hex: 0xF0000000
             Bin: 1111 0000 0000 0000 0000 0000 0000 0000
             (Restored to original)

    [Test 2] Data: 0x12345678 -> ROR 8 -> 0x78123456
    ```
 */
void test_lr(uint32_t n, unsigned int steps);

void print_binary(uint32_t n);
uint32_t rotate(uint32_t n, unsigned int steps, bool dir);

int main(void)
{
    printf("=== Day 6: Circular Shift (Rotate) ===\n\n");
    uint32_t sample = 0xF0000000;
    test_lr(sample, 4);
    // Test 2
    sample = 0x12345678;
    printf("\n\n[Test 2] Data: 0x%08X -> ROR 8 -> 0x%08X\n", sample, rotate(sample, 8, 1));
    return 0;
}

void print_binary(uint32_t n)
{
    for (int bit = 31; bit >= 0; --bit)
    {
        putchar(((unsigned)n >> bit) & 1U ? '1' : '0');
        // print 1 whitespace for every 4 bits
        if (bit > 0 && bit % 4 == 0)
        {
            putchar(' ');
        }
    }
}

void test_lr(uint32_t n, unsigned int steps)
{
    // Initial Phase
    printf("[Init]   Hex: 0x%08X\n", n);
    printf("         Bin: ");
    print_binary(n);
    // Rotate Left
    uint32_t result = rotate(n, steps, 0);
    printf("\n\n[ROL %u]  Hex: 0x%08X\n", steps % 32U, result);
    printf("         Bin: "); // 9 spaces
    print_binary(result);
    printf("\n         (MSB bits moved to LSB)");

    // Rotate Right
    result = rotate(result, steps, 1);
    printf("\n\n[ROR %u]  Hex: 0x%08X\n", steps % 32U, result);
    printf("         Bin: "); // 9 spaces
    print_binary(result);
    printf("\n         (Restored to original)");
}

// rotate:  32비트 정수, 이동할 비트 수 n, 방향(Left/Right)
// n:       uint32_t
// steps:   0 <= steps <= 31
// dir:     0: left, 1: right
uint32_t rotate(uint32_t n, unsigned int steps, bool dir)
{
    // 1. 32-bit rotation
    // e.g. 32 -> 0, 33 -> 1, 36 -> 4
    steps %= 32U;

    // Avoid shifting by 32
    if (steps == 0U)
    {
        return n;
    }

    // 2. Rotation

    /** abcd₂
     * Rotate right by 2:
     * 1. abcd₂ >> 2 = 00ab₂
     * 2. abcd₂ << 2 = cd00₂
     * 3. 00ab₂ | cd00₂ = cdab₂
     *
     * Rotate left by 2:
     * 1. abcd₂ << 2 = cd00₂
     * 2. abcd₂ >> 2 = 00ab₂
     * 3. cd00₂ | 00ab₂ = cdab₂
     */

    // Rotate Right
    if (dir)
    {
        return (n >> steps) | (n << (32U - steps));
    }
    // Rotate Left
    else
    {
        return (n << steps) | (n >> (32U - steps));
    }
}


````
