/* 김현수 제출 코드 기반. 원문: originals/day11.md
 * 입력 메뉴는 공통 main으로 이동. 검사와 결정적 예제를 추가했다.
 */
#include "calculator.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
_Static_assert(sizeof(int) <= 4, "Calculator requires int of at most 32 bits");
typedef int64_t (*OperationFunction)(int64_t, int64_t);
static int64_t add(int64_t a, int64_t b) { return a + b; }
static int64_t sub(int64_t a, int64_t b) { return a - b; }
static int64_t mul(int64_t a, int64_t b) { return a * b; }
static int64_t divide(int64_t a, int64_t b) { return a / b; }

bool rp11_calculate(rp11_Operation op, int a, int b, int *result)
{
    static const OperationFunction operations[] = {add, sub, mul, divide};
    /* Validation may branch; operation selection remains table driven. */
    if (result == NULL || op < RP11_ADD || op > RP11_DIV ||
        (op == RP11_DIV && b == 0)) return false;
    const int64_t value = operations[op](a, b);
    if (value < INT_MIN || value > INT_MAX) return false;
    *result = (int)value;
    return true;
}

int rp11_demo(void)
{
    const int inputs[][2] = {{1, 2}, {2, 1}, {2, 1}, {1, 2}};
    puts("=== Day 11: Function Pointer Array Calculator ===");
    for (int op = RP11_ADD; op <= RP11_DIV; ++op) {
        int result;
        if (!rp11_calculate((rp11_Operation)op, inputs[op][0], inputs[op][1], &result))
            return 1;
        printf("Operation %d (%d, %d) >> Result: %d\n",
               op, inputs[op][0], inputs[op][1], result);
    }
    return 0;
}
