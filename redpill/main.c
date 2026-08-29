#include "bits.h"
#include "popcount.h"
#include "rotate.h"
#include "checksum.h"
#include "memmove.h"
#include "swap.h"
#include "matrix.h"
#include "calculator.h"
#include "offset.h"
#include "pool.h"
#include "hexdump.h"
#include "ring_buffer.h"
#include "linked_list.h"
#include "bitmap.h"
#include "min_heap.h"
#include "timer.h"
#include "tokenizer.h"
#include "debounce.h"
#include "moving_average.h"
#include "producer_consumer.h"
#include "uart_chat.h"
#include "tests/selftest.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int day; const char *name; int (*run)(void); } Demo;
static const Demo demos[] = {
    {4, "bits", rp04_demo},
    {5, "popcount", rp05_demo},
    {6, "rotate", rp06_demo},
    {7, "checksum", rp07_demo},
    {8, "memmove", rp08_demo},
    {9, "swap", rp09_demo},
    {10, "matrix", rp10_demo},
    {11, "calculator", rp11_demo},
    {12, "offset", rp12_demo},
    {13, "pool", rp13_demo},
    {14, "hexdump", rp14_demo},
    {15, "ring_buffer", rp15_demo},
    {16, "linked_list", rp16_demo},
    {17, "bitmap", rp17_demo},
    {18, "min_heap", rp18_demo},
    {22, "timer", rp22_demo},
    {23, "tokenizer", rp23_demo},
    {24, "debounce", rp24_demo},
    {25, "moving_average", rp25_demo},
    {26, "producer_consumer", rp26_demo},
    {33, "uart_chat", rp33_demo},
};
static void list_demos(void)
{
    puts("Redpill / 김현수 (Alex) — available class modules:");
    for (size_t i = 0; i < sizeof demos / sizeof demos[0]; ++i)
        printf("  %2d  %s\n", demos[i].day, demos[i].name);
}
int main(int argc, char **argv)
{
    if (argc > 2) {
        fputs("Usage: redpill_demo [--all | --list | --test | DAY]\n", stderr);
        return EXIT_FAILURE;
    }
    if (argc == 2 && strcmp(argv[1], "--list") == 0) { list_demos(); return 0; }
    if (argc == 2 && strcmp(argv[1], "--test") == 0) return redpill_selftest();
    if (argc == 1 || strcmp(argv[1], "--all") == 0) {
        for (size_t i = 0; i < sizeof demos / sizeof demos[0]; ++i) {
            printf("\n--- Day %d / %s ---\n", demos[i].day, demos[i].name);
            if (demos[i].run() != 0) return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    char *end = NULL;
    errno = 0;
    const long day = strtol(argv[1], &end, 10);
    if (errno == 0 && end != argv[1] && *end == '\0') {
        for (size_t i = 0; i < sizeof demos / sizeof demos[0]; ++i)
            if (day == demos[i].day) return demos[i].run();
    }
    fprintf(stderr, "Unknown day/option: %s. Use --list.\n", argv[1]);
    return EXIT_FAILURE;
}
