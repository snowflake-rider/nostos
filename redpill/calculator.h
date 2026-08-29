/* 김현수 Day 11의 enum + 함수 포인터 배열을 분리한 API. */
#ifndef REDPILL_CALCULATOR_H
#define REDPILL_CALCULATOR_H
#include <stdbool.h>
typedef enum { RP11_ADD, RP11_SUB, RP11_MUL, RP11_DIV } rp11_Operation;
/* Invalid operation, overflow, zero division: false; result is unchanged. */
bool rp11_calculate(rp11_Operation op, int a, int b, int *result);
int rp11_demo(void);
#endif
