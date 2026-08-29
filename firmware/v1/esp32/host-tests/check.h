#ifndef TEST_CHECK_H
#define TEST_CHECK_H
#include <stdio.h>
#include <stdlib.h>
/* Release에서도 검사가 생략되지 않는다. */
#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#endif
