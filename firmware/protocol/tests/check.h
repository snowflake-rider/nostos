#ifndef TEST_CHECK_H
#define TEST_CHECK_H
#include <stdio.h>
#include <stdlib.h>
/* Release의 NDEBUG와 무관하게 실패를 검출한다. */
#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#endif
