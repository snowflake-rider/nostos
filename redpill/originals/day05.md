# Day 5 — 김현수 원문

출처: https://app.notion.com/b1c2ae70087183f4b0a881685df5fd5f

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdint.h>
#include <stdio.h>
/**
- **입력:** 32비트 정수
- **출력:** 켜져 있는 비트(1)의 개수
- **제약조건:** 루프(`for/while`) 없이 비트 연산만으로 구현 (Brian Kernighan
알고리즘 등 활용).
=== Day 5: Population Count (Counting Set Bits) ===

Case 1: Input 0x00000000
  [Naive]     : 0
  [Kernighan] : 0 (Recommended Logic)
  [SWAR]      : 0 (Strict Loop-free)
  [Built-in]  : 0
------------------------------
Case 2: Input 0x00000007
  [Naive]     : 3
  [Kernighan] : 3 (Recommended Logic)
  [SWAR]      : 3 (Strict Loop-free)
  [Built-in]  : 3
------------------------------
Case 3: Input 0x12345678
  [Naive]     : 13
  [Kernighan] : 13 (Recommended Logic)
  [SWAR]      : 13 (Strict Loop-free)
  [Built-in]  : 13
------------------------------
Case 4: Input 0xFFFFFFFF
  [Naive]     : 32
  [Kernighan] : 32 (Recommended Logic)
  [SWAR]      : 32 (Strict Loop-free)
  [Built-in]  : 32
------------------------------
 */

unsigned int popcount_naive(uint32_t n);
unsigned int popcount_swar(uint32_t n);
unsigned int popcount_brian(uint32_t n);
unsigned int popcount_builtin(uint32_t n);
void print_case(unsigned int case_number, uint32_t input);
void print_binary(uint32_t n);

int main(void)
{
  uint32_t samples[4] = {0x00000000, 0x00000007, 0x12345678, 0xFFFFFFFF};
  for (uint8_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
  {
    print_case((unsigned)i + 1U, samples[i]);
  }
  return 0;
}

unsigned int popcount_naive(uint32_t n)
{
  unsigned int count = 0;
  for (int bit = 0; bit < 32; ++bit)
  {
    count += (n >> bit) & 1U;
  }
  return count;
}

unsigned int popcount_builtin(uint32_t n)
{
  return (unsigned int)__builtin_popcount((unsigned)n);
}

void print_case(unsigned int case_number, uint32_t input)
{
  printf("Case %u: Input 0x%08X\n", case_number, (unsigned)input);
  printf("  [Naive]     : %u\n", popcount_naive(input));
  printf("  [Kernighan] : %u (Recommended Logic)\n", popcount_brian(input));
  printf("  [SWAR]      : %u (Strict Loop-free)\n", popcount_swar(input));
  printf("  [Built-in]  : %u\n", popcount_builtin(input));
  printf("------------------------------\n");
}

/*
 * SWAR: SIMD Within A Register
 *
 * SIMD performs the same operation on multiple values simultaneously.
 * SWAR treats groups of bits within one integer as separate lanes.
 *
 * popcount_swar(ab₂)   = a + b
 * popcount_swar(abcd₂) = a + b + c + d
 *
 * Let "ab₂" be a 2-bit binary number where a and b are bits (0 or 1).
 *
 *     ab₂ = 2a + b
 *     ab₂ >> 1 = a
 *     ab₂ - (ab₂ >> 1) = (2a + b) - a = a + b = popcount_swar(ab₂)
 *
 * Let "abcd₂" be a 4-bit binary number where a, b, c, and d are bits.
 *
 *     abcd₂ = 8a + 4b + 2c + d
 *     abcd₂ >> 1 = 0abc₂
 *     0abc₂ & 0x5 = 0a0c₂ (0x5 = 0101₂)
 *     abcd₂ & 0x5 = 0b0d₂
 *     0a0c₂ + 0b0d₂ = [a + b] | [c + d] = qwer₂
 *     (qw₂ = popcount_swar(ab₂) = a + b,
 *      er₂ = popcount_swar(cd₂) = c + d)
 *     1. (qwer₂ >> 2) & 0x3 = qw₂ (0x3 = 0011₂)
 *     2.        qwer₂ & 0x3 = er₂
 *     Since popcount_swar(abcd₂)
 *         = popcount_swar(ab₂) + popcount_swar(cd₂)
 *         = qw₂ + er₂
 *         = a + b + c + d
 *    1. n = n - ((n >> 1) & 0x5)
 *    2. return ((n >> 2) & 0x3) + (n & 0x3)
 */
unsigned int popcount_swar(uint32_t n)
{
  // n is 32 bits
  // 1. shift by 1 to move the higher bit of each 2-bit block to the lower bit
  // 2. mask with 0101...0101₂ = 0x55555555
  // 3. subtract from n
  // 4. n = [A][B][C][D]...[P], where each 2-bit block contains the
  //    population count of 2 consecutive bits of the original n
  //    e.g. original n = 00011011...₂ -> A=00₂, B=01₂, C=01₂, D=10₂
  n = n - ((n >> 1) & 0x55555555);

  // 1. shift by 2 to align neighboring 2-bit population counts
  // 2. mask with 0011...0011₂ = 0x33333333
  // 3. add the neighboring 2-bit population counts
  // 4. n = [A][B][C][D][E][F][G][H], where each 4-bit block contains the
  //    population count of 4 consecutive bits of the original n
  n = ((n >> 2) & 0x33333333) + (n & 0x33333333);

  // 1. shift by 4 to align neighboring 4-bit population counts
  // 2. mask with 0000 1111 0000 1111 0000 1111 0000 1111 = 0x0F0F0F0F
  // 3. add the neighboring 4-bit population counts
  // 4. n = [A][B][C][D], where each 8-bit block contains the population
  //    count of 8 consecutive bits of the original n
  n = ((n >> 4) & 0x0F0F0F0F) + (n & 0x0F0F0F0F);

  // 1. shift by 8 to align neighboring 8-bit population counts
  // 2. add the neighboring 8-bit population counts
  // 3. the lowest 8-bit block contains the population count of the lower
  //    16 bits of the original n
  n = (n >> 8) + n;

  // 1. shift by 16 to align the upper and lower 16-bit population counts
  // 2. add the two 16-bit population counts
  // 3. the lowest 8-bit block contains the population count of all 32 bits
  n = (n >> 16) + n;

  // 1. the maximum population count is 32 = 100000₂, which needs 6 bits
  // 2. mask the lowest 6 bits with 0011 1111₂ = 0x3F
  return n & 0x3F;
}
// max return value: 32
unsigned int popcount_brian(uint32_t n)
{
  // Brian's Logic
  // 1. clear the least-significant set bit.
  // (n-1)          = xxxx1000 - 1 = xxxx0111
  // 2. n & (n-1)   = xxxx0000 => cleared the last bit
  unsigned int counts = 0;
  for (uint32_t x = n; x > 0; counts++)
  {
    x = x & (x - 1);
  }
  return counts;
}


````
