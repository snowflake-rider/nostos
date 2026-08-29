# Day 4 — 김현수 원문

출처: https://app.notion.com/d7c2ae7008718339a31f015459b4d577

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdint.h>
#include <stdio.h>

/**
 * 비트 단위 Reverse (Mirroring)
 * - **입력:** 8비트 정수 `0b11010010`
 * - **출력:** `0b01001011` (비트 순서 반전)
 * - **제약조건:** Lookup Table(LUT)을 사용하지 않고 O(1) 비트 연산으로 구현.
 */

uint8_t reverse_bits(uint8_t x);
void print_bits(uint8_t value);
void run_case(unsigned int case_number, uint8_t input, uint8_t expected);

int main(void) {
  printf("=== Day 4: Bitwise Reverse (Mirroring) ===\n\n");

  run_case(1U, 0xD2U, 0x4BU);
  run_case(2U, 0x0FU, 0xF0U);
  run_case(3U, 0xAAU, 0x55U);
  run_case(4U, 0x12U, 0x48U);

  return 0;
}

uint8_t reverse_bits(uint8_t x) {

  uint8_t reversed = 0U;
  // 4bits example
  // reversed | x
  // 0000     | abcd
  // 000d     | 0abc
  // 00dc     | 00ab
  // 0dcb     | 000a
  // dcba     | 0000
  for (uint8_t i = 0U; i < 8U; ++i) {
    // 1. Shift Reversed left by 1
    reversed = reversed << 1U; // reversed <<= 1U
    // 2. Get the last bit of x (masking)
    reversed = reversed | (x & 1U);
    // 3. Shift x right by 1
    x = x >> 1U;
  }

  return reversed;
}

void print_bits(uint8_t value) {
  for (int bit = 7; bit >= 0; --bit) {
    printf("%u", (unsigned int)((value >> bit) & 1U));
    if (bit == 4) {
      printf(" ");
    }
  }
}

void run_case(unsigned int case_number, uint8_t input, uint8_t expected) {
  uint8_t output = reverse_bits(input);

  printf("Case %u:\n", case_number);
  printf("  Input : 0x%02X (", (unsigned int)input);
  print_bits(input);
  printf(")\n");
  printf("  Output: 0x%02X (", (unsigned int)output);
  print_bits(output);
  printf(")\n");
  printf("  Verify: %s\n", output == expected ? "OK" : "FAIL");
  printf("------------------------\n");
}


````
