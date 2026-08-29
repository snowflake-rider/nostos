/* 김현수 제출 코드 기반. 원문: originals/day09.md */
#include "swap.h"

#include <stdio.h>
#include <stddef.h>
/**
 * Day 9. 제네릭 Swap 함수 (void)*
 * - **입력:** 두 변수의 주소 `void *a, void *b`, 데이터 크기 `size_t size`
- **출력:** 두 변수의 값이 교환됨
- **제약조건:** `malloc` 사용 금지, 바이트 단위 교환 알고리즘 사용.
- **실행결과:**

```c
=== Day 9: Generic Swap Implementation ===

[Int] Before: 10, 20
[Int] After : 20, 10

[Double] Before: 3.14159, 99.99000
[Double] After : 99.99000, 3.14159

[Struct] Before: Kim(1), Lee(2)
[Struct] After : Lee(2), Kim(1)
```
 */

// STRUCT PERSON
typedef struct rp09_Person
{
    char lname[20];
    int id;
} rp09_Person;

/*
 * PairPrinter is a function-pointer type.
 *
 * Read void (*PairPrinter)(const void *a, const void *b) from the name outward:
 * 1. (*PairPrinter)                  PairPrinter is a pointer.
 * 2. (const void *a, const void *b)  It points to a function taking two
 *                                    pointers to const objects.
 * 3. void                            The function returns no value.
 *
 * The parentheses around *PairPrinter are required. Without them, the
 * declaration would describe a function returning void * instead.
 */
typedef void (*rp09_PairPrinter)(const void *a, const void *b);

// HELPERS
void rp09_my_swap(void *a, void *b, size_t size);
static void print_pair(const char *type_name, const char *stage, rp09_PairPrinter printer, const void *a, const void *b);
static void print_int_pair(const void *a, const void *b);
static void print_double_pair(const void *a, const void *b);
static void print_person_pair(const void *a, const void *b);

// MAIN DRIVER
int rp09_demo(void)
{
    printf("=== Day 9: Generic Swap Implementation ===\n\n");
    // CASE: INT
    int int_a = 10, int_b = 20;
    print_pair("Int", "Before", print_int_pair, &int_a, &int_b);
    rp09_my_swap(&int_a, &int_b, sizeof int_a);
    print_pair("Int", "After", print_int_pair, &int_a, &int_b);
    putchar('\n');

    // CASE: DOUBLE
    double db_a = 3.14159, db_b = 99.99000;
    print_pair("Double", "Before", print_double_pair, &db_a, &db_b);
    rp09_my_swap(&db_a, &db_b, sizeof db_a);
    print_pair("Double", "After", print_double_pair, &db_a, &db_b);
    putchar('\n');

    // CASE: STRUCT PERSON
    rp09_Person ps_a = {.lname = "Kim", .id = 1}, ps_b = {.lname = "Lee", .id = 2};
    print_pair("Struct", "Before", print_person_pair, &ps_a, &ps_b);
    rp09_my_swap(&ps_a, &ps_b, sizeof ps_a);
    print_pair("Struct", "After", print_person_pair, &ps_a, &ps_b);

    return 0;
}
/*
 * my_swap: swaps two values byte by byte.
 *
 *     Before:  a --> [ A bytes ]    b --> [ B bytes ]
 *     After :  a --> [ B bytes ]    b --> [ A bytes ]
 *
 * void * stores an address, but not the original type or size.
 * Therefore, the caller guarantees:
 *
 * 1. a and b --> at least size writable bytes.
 * 2. Their memory regions do not partially overlap.
 *    a == b --> allowed; nothing needs to be swapped.
 * 3. For a value swap:
 *    a and b --> objects of the same type
 *    size    --> sizeof that object type
 *
 * Example: my_swap(&value_a, &value_b, sizeof value_a);
 */
void rp09_my_swap(void *a, void *b, size_t size)
{
    // CASE: a, b point to same memory address or size is 0
    if (a == b || size == 0)
    {
        return;
    }

    // CASE: a, b swap byte by byte iteratively
    unsigned char *p = a;
    unsigned char *q = b;
    for (size_t i = 0; i < size; i++)
    {
        unsigned char tmp = p[i];
        p[i] = q[i];
        q[i] = tmp;
    }
}
// print_pair: a generic print function for printing a pair of (A, B)
static void print_pair(const char *type_name, const char *stage, rp09_PairPrinter printer, const void *a, const void *b)
{
    printf("[%s] %-6s: ", type_name, stage);
    printer(a, b);
    putchar('\n');
}
// print_int_pair: prints a pair of INTs
static void print_int_pair(const void *a, const void *b)
{
    printf("%d, %d", *(const int *)a, *(const int *)b);
}
// print_double_pair: prints a pair of DOUBLEs
static void print_double_pair(const void *a, const void *b)
{
    printf("%.5f, %.5f", *(const double *)a, *(const double *)b);
}
// print_person_pair: prints a pair of struct PERSONs
static void print_person_pair(const void *a, const void *b)
{
    const rp09_Person *person_a = a;
    const rp09_Person *person_b = b;
    printf("%s(%d), %s(%d)", person_a->lname, person_a->id, person_b->lname, person_b->id);
}
