# Day 8 — 김현수 원문

출처: https://app.notion.com/fef2ae7008718339a88a819672ddc5ae

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Input: Source Address, Destination Address, Bytes (to copy)

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))
static const uint8_t TEST_DATA[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

uint8_t *my_memmove(uint8_t *dest, const uint8_t *src, size_t byte_count);
void print_bytes(const char *prefix, const uint8_t *src, size_t size);
void verify_bytes(const uint8_t *actual, const uint8_t *expected, size_t size);
int main(void)
{
    printf("=== Day 8: Safe Memcpy (memmove) Implementation ===\n\n");

    // Test 1: Overlap (Dest > Src) -> Shift Right 2 bytes

    uint8_t actual[ARRAY_LENGTH(TEST_DATA)];
    uint8_t expected[ARRAY_LENGTH(TEST_DATA)];
    memcpy(actual, TEST_DATA, sizeof actual);
    memcpy(expected, TEST_DATA, sizeof expected);

    print_bytes("[Initial] ", actual, ARRAY_LENGTH(TEST_DATA));
    // Test my_memmove
    my_memmove(actual + 2, actual, 5);
    // Execute memmove to verify the result
    memmove(expected + 2, expected, 5);

    // Verify
    printf("\nTest 1: Overlap (Dest > Src) -> Shift Right 2 bytes\n");
    print_bytes("[Result ] ", actual, ARRAY_LENGTH(TEST_DATA));
    verify_bytes(actual, expected, sizeof actual);

    // Test 2: Overlap (Dest < Src) -> Shift Left 2 bytes
    memcpy(actual, TEST_DATA, sizeof actual);
    memcpy(expected, TEST_DATA, sizeof expected);
    print_bytes("\n[Initial] ", actual, ARRAY_LENGTH(actual));
    // Test my_memmove
    my_memmove(actual, actual + 2, 5);
    // Execute memmove to verify the result
    memmove(expected, expected + 2, 5);
    // Verify
    printf("Test 2: Overlap (Dest < Src) -> Shift Left 2 bytes\n");
    print_bytes("[Result ] ", actual, ARRAY_LENGTH(actual));
    verify_bytes(actual, expected, sizeof actual);
    //
    return 0;
}
uint8_t *my_memmove(uint8_t *dest, const uint8_t *src, size_t byte_count)
{
    if (dest == src || byte_count == 0)
    {
        return dest;
    }
    if (dest > src && dest < src + byte_count)
    {
        // Destination overlaps the src from the right
        for (size_t i = byte_count; i > 0; --i)
        {
            dest[i - 1] = src[i - 1];
        }
    }
    else
    {
        for (size_t i = 0; i < byte_count; ++i)
        {
            dest[i] = src[i];
        }
    }
    return dest;
}
void print_bytes(const char *prefix, const uint8_t *src, size_t size)
{
    printf("%s", prefix);
    for (size_t i = 0; i < size; ++i)
    {
        printf("%02X", src[i]);
        if (i + 1 < size)
        {
            putchar(' ');
        }
    }
    putchar('\n');
}
void verify_bytes(const uint8_t *actual, const uint8_t *expected, size_t size)
{
    if (memcmp(actual, expected, size) == 0)
    {
        printf(">> Success!\n");
    }
    else
    {
        printf(">> Failed!\n");
        print_bytes("[Actual   ] ", actual, size);
        print_bytes("[Expected ] ", expected, size);
    }
}

````
