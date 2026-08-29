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
#include "selftest.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int test_bits_memory(void)
{
    for (unsigned int x = 0; x <= 255; ++x) {
        unsigned int expected = 0;
        for (unsigned int b = 0; b < 8; ++b)
            expected |= ((x >> b) & 1U) << (7U - b);
        CHECK(rp04_reverse_bits((uint8_t)x) == expected);
    }
    uint32_t x = 0;
    for (unsigned int i = 0; i < 1024; ++i) {
        x = x * UINT32_C(1664525) + UINT32_C(1013904223);
        CHECK(rp05_popcount_naive(x) == rp05_popcount_swar(x));
        CHECK(rp05_popcount_naive(x) == rp05_popcount_brian(x));
        CHECK(rp05_popcount_naive(x) == rp05_popcount_builtin(x));
        for (unsigned int n = 0; n <= 64; ++n) {
            uint32_t rotated = x;
            for (unsigned int bit = 0; bit < n; ++bit)
                rotated = (rotated << 1) | (rotated >> 31);
            CHECK(rp06_rotate(x, n, false) == rotated);
            CHECK(rp06_rotate(rotated, n, true) == x);
        }
    }
    CHECK(rp05_popcount_swar(0) == 0);
    CHECK(rp05_popcount_swar(UINT32_MAX) == 32);
    CHECK(rp06_rotate(0x12345678U, UINT_MAX, false) ==
          rp06_rotate(0x12345678U, UINT_MAX % 32U, false));
    uint8_t packet[] = {1, 4, 0x10, 0x20, 0x30, 0x40, 0};
    rp07_update_packet_checksum(packet, 6);
    CHECK(packet[6] == 0x45);
    CHECK(rp07_xor_checksum(packet, sizeof packet) == 0);
    packet[2] = 0xef;
    CHECK(rp07_xor_checksum(packet, sizeof packet) == 0xff);
    CHECK(rp07_xor_checksum(NULL, 0) == 0);

    for (size_t src = 0; src < 16; ++src)
        for (size_t dst = 0; dst < 16; ++dst)
            for (size_t count = 0; count <= 16 - src && count <= 16 - dst; ++count) {
                uint8_t actual[16], expected[16];
                for (size_t i = 0; i < sizeof actual; ++i) actual[i] = (uint8_t)i;
                memcpy(expected, actual, sizeof actual);
                CHECK(rp08_my_memmove(actual + dst, actual + src, count) == actual + dst);
                memmove(expected + dst, expected + src, count);
                CHECK(memcmp(actual, expected, sizeof actual) == 0);
            }
    uint8_t source[] = {1, 2, 3}, destination[3];
    rp08_my_memmove(destination, source, sizeof source);
    CHECK(memcmp(destination, source, sizeof source) == 0);
    CHECK(rp08_my_memmove(NULL, NULL, 0) == NULL);
    int a = 12, b = -5;
    rp09_my_swap(&a, &b, sizeof a);
    CHECK(a == -5 && b == 12);
    rp09_my_swap(&a, &a, sizeof a);
    CHECK(a == -5);
    rp09_my_swap(NULL, NULL, 0);
    int **matrix = rp10_create_matrix(3, 4);
    CHECK(matrix != NULL);
    CHECK(matrix[1] == matrix[0] + 4 && matrix[2] == matrix[0] + 8);
    matrix[2][3] = 99;
    CHECK(matrix[0][11] == 99);
    rp10_free_matrix(matrix);
    CHECK(rp10_create_matrix(0, 1) == NULL);
    CHECK(rp10_create_matrix(1, 0) == NULL);
    CHECK(rp10_create_matrix(SIZE_MAX, 2) == NULL);
    CHECK(rp10_create_matrix(2, SIZE_MAX) == NULL);
    int value = 999;
    CHECK(rp11_calculate(RP11_ADD, 1, 2, &value) && value == 3);
    CHECK(rp11_calculate(RP11_SUB, 2, 1, &value) && value == 1);
    CHECK(rp11_calculate(RP11_MUL, -2, 3, &value) && value == -6);
    CHECK(rp11_calculate(RP11_DIV, 1, 2, &value) && value == 0);
    value = 999;
    CHECK(!rp11_calculate(RP11_ADD, INT_MAX, 1, &value));
    CHECK(!rp11_calculate(RP11_SUB, INT_MIN, 1, &value));
    CHECK(!rp11_calculate(RP11_MUL, INT_MAX, INT_MAX, &value));
    CHECK(!rp11_calculate(RP11_DIV, 1, 0, &value));
    CHECK(!rp11_calculate(RP11_DIV, INT_MIN, -1, &value));
    CHECK(!rp11_calculate((rp11_Operation)99, 1, 1, &value));
    CHECK(!rp11_calculate(RP11_ADD, 1, 1, NULL));
    CHECK(value == 999);
    struct OffsetProbe { char a; int b; double c; } probe = {0};
    CHECK(RP12_MEMBER_OFFSET(probe, a) == offsetof(struct OffsetProbe, a));
    CHECK(RP12_MEMBER_OFFSET(probe, b) == offsetof(struct OffsetProbe, b));
    CHECK(RP12_MEMBER_OFFSET(probe, c) == offsetof(struct OffsetProbe, c));
    return 0;
}

static int test_containers(void)
{
    rp13_MemoryPool pool;
    rp13_pool_init(&pool);
    void *blocks[rp13_POOL_SIZE];
    for (size_t i = 0; i < rp13_POOL_SIZE; ++i) {
        blocks[i] = rp13_pool_alloc(&pool);
        CHECK(blocks[i] != NULL);
        CHECK((uintptr_t)blocks[i] % _Alignof(max_align_t) == 0);
        memset(blocks[i], 0xa5, rp13_BLOCK_SIZE);
    }
    CHECK(pool.used_count == rp13_POOL_SIZE);
    CHECK(rp13_pool_alloc(&pool) == NULL);
    int foreign;
    CHECK(!rp13_pool_free(&pool, &foreign));
    CHECK(!rp13_pool_free(&pool, (uint8_t *)blocks[0] + 1));
    CHECK(rp13_pool_free(&pool, blocks[3]));
    CHECK(!rp13_pool_free(&pool, blocks[3]));
    CHECK(rp13_pool_alloc(&pool) == blocks[3]);
    for (size_t i = 0; i < rp13_POOL_SIZE; ++i) CHECK(rp13_pool_free(&pool, blocks[i]));
    CHECK(pool.used_count == 0);
    CHECK(rp13_pool_alloc(NULL) == NULL);

    rp15_RingBuffer rb;
    rp15_rb_init(&rb);
    uint8_t out = 77;
    CHECK(!rp15_rb_get(&rb, &out) && out == 77);
    for (unsigned int round = 0; round < 12; ++round) {
        for (uint8_t i = 0; i < 7; ++i) CHECK(rp15_rb_put(&rb, i));
        CHECK(!rp15_rb_put(&rb, 99));
        for (uint8_t i = 0; i < 7; ++i) CHECK(rp15_rb_get(&rb, &out) && out == i);
        CHECK(!rp15_rb_get(&rb, &out));
    }
    const int values[] = {1, 2, 3}, reversed[] = {3, 2, 1};
    rp16_LinkedList *list = rp16_init(values, 3);
    CHECK(list != NULL);
    rp16_Node *old_head = list->head;
    rp16_reverse(list);
    CHECK(rp16_ll_equals_array(list, reversed, 3));
    CHECK(list->head->next->next == old_head);
    rp16_reverse(list);
    CHECK(rp16_ll_equals_array(list, values, 3));
    rp16_destroy(list);
    list = rp16_init(NULL, 0);
    CHECK(list != NULL);
    rp16_reverse(list);
    CHECK(rp16_ll_equals_array(list, NULL, 0));
    rp16_destroy(list);
    CHECK(rp16_init(NULL, 1) == NULL);

    for (size_t capacity = 0; capacity <= rp17_NUM_RESOURCES; ++capacity) {
        rp17_Bitmap bm;
        CHECK(rp17_bitmap_init(&bm, capacity));
        size_t index = 999;
        for (size_t i = 0; i < capacity; ++i)
            CHECK(rp17_bitmap_alloc(&bm, &index) && index == i);
        CHECK(!rp17_bitmap_alloc(&bm, &index));
        CHECK(!rp17_bitmap_free(&bm, capacity));
        if (capacity > 0) {
            CHECK(rp17_bitmap_free(&bm, 0));
            CHECK(!rp17_bitmap_free(&bm, 0));
            CHECK(rp17_bitmap_alloc(&bm, &index) && index == 0);
        }
    }
    rp17_Bitmap bm;
    CHECK(!rp17_bitmap_init(&bm, rp17_NUM_RESOURCES + 1));
    rp18_TaskScheduler heap = {0};
    CHECK(rp18_init_task_scheduler(&heap, 0));
    rp18_Task task;
    CHECK(!rp18_extract(&heap, &task));
    for (unsigned int i = 0; i < 100; ++i) {
        task = (rp18_Task){(uint8_t)i, (uint8_t)(99U - i)};
        CHECK(rp18_insert(&heap, &task));
    }
    CHECK(!rp18_insert(&heap, &task));
    for (unsigned int i = 0; i < 100; ++i)
        CHECK(rp18_extract(&heap, &task) && task.priority == i);
    CHECK(!rp18_extract(&heap, &task));
    free(heap.tasks);

    /* Inserting an existing element must survive realloc moving the array. */
    CHECK(rp18_init_task_scheduler(&heap, 1));
    task = (rp18_Task){42, 3};
    CHECK(rp18_insert(&heap, &task));
    CHECK(rp18_insert(&heap, &heap.tasks[0]));
    CHECK(heap.task_count == 2);
    for (unsigned int i = 0; i < 2; ++i)
        CHECK(rp18_extract(&heap, &task) && task.task_id == 42 && task.priority == 3);
    free(heap.tasks);
    return 0;
}

static unsigned int timer_ids[8], timer_times[8], timer_events, current_tick;
static void record_timer(unsigned int id)
{
    if (timer_events < 8) {
        timer_ids[timer_events] = id;
        timer_times[timer_events] = current_tick;
    }
    ++timer_events;
}
static int test_streams(void)
{
    timer_events = 0;
    rp22_InitTimerScheduler(record_timer);
    rp22_SetTimer(1, 10);
    rp22_SetTimer(2, 5);
    rp22_SetTimer(3, 10);
    rp22_SetTimer(4, 1);
    rp22_SetTimer(5, 0);
    for (current_tick = 1; current_tick <= 12; ++current_tick) rp22_Tick();
    CHECK(timer_events == 4);
    CHECK(timer_ids[0] == 4 && timer_times[0] == 1);
    CHECK(timer_ids[1] == 2 && timer_times[1] == 5);
    CHECK(timer_ids[2] == 1 && timer_times[2] == 10);
    CHECK(timer_ids[3] == 3 && timer_times[3] == 10);
    rp22_InitTimerScheduler(record_timer); /* All timer allocations were freed. */

    const char text[] = ",,GPS,,37.5,";
    rp23_Tokenizer tokenizer = {.source = text, .delimiter = ','};
    rp23_Token token;
    CHECK(rp23_next_token(&tokenizer, &token));
    CHECK(token.length == 3 && memcmp(token.start, "GPS", 3) == 0);
    CHECK(rp23_next_token(NULL, &token));
    CHECK(token.length == 4 && memcmp(token.start, "37.5", 4) == 0);
    CHECK(!rp23_next_token(NULL, &token));
    CHECK(!rp23_next_token(NULL, &token));
    CHECK(strcmp(text, ",,GPS,,37.5,") == 0);
    rp23_Tokenizer empty = {.source = "", .delimiter = ','};
    CHECK(!rp23_next_token(&empty, &token));

    rp24_Debouncer db;
    CHECK(!rp24_init_debouncer(&db, 0));
    CHECK(rp24_init_debouncer(&db, 3));
    CHECK(!rp24_debounce(&db, true) && db.counter == 1);
    CHECK(!rp24_debounce(&db, false) && db.counter == 0);
    CHECK(!rp24_debounce(&db, true));
    CHECK(!rp24_debounce(&db, true));
    CHECK(rp24_debounce(&db, true) && db.stable_output);
    CHECK(!rp24_debounce(&db, false));
    CHECK(!rp24_debounce(&db, false));
    CHECK(rp24_debounce(&db, false) && !db.stable_output);

    rp25_SlidingWindow sw;
    rp25_sliding_window_init(&sw);
    CHECK(isnan(rp25_moving_average(&sw)));
    /* Original warm-up rule: do not gate the first five finite samples. */
    for (int i = 0; i < 5; ++i) CHECK(rp25_sliding_window_update(&sw, 20.0f));
    CHECK(rp25_moving_average(&sw) == 20.0f);
    const size_t index = sw.buffer_idx;
    CHECK(!rp25_sliding_window_update(&sw, 31.0f));
    CHECK(!rp25_sliding_window_update(&sw, NAN));
    CHECK(!rp25_sliding_window_update(&sw, INFINITY));
    CHECK(sw.buffer_idx == index && sw.window_sum == 100.0 && sw.count == 5);
    CHECK(rp25_sliding_window_update(&sw, 30.0f)); /* Inclusive 50% boundary. */
    CHECK(rp25_moving_average(&sw) == 22.0f);
    rp25_sliding_window_init(&sw);
    for (int i = 0; i < 5; ++i) CHECK(rp25_sliding_window_update(&sw, -20.0f));
    CHECK(!rp25_sliding_window_update(&sw, -31.0f));
    CHECK(rp25_sliding_window_update(&sw, -10.0f));
    CHECK(rp25_moving_average(&sw) == -18.0f);
    rp25_sliding_window_init(&sw);
    for (int i = 0; i < 5; ++i) CHECK(rp25_sliding_window_update(&sw, 0.0f));
    CHECK(!rp25_sliding_window_update(&sw, 1.0f));

    rp26_Manager manager = {.next_item = 1};
    CHECK(!rp26_consume(&manager));
    for (unsigned int i = 1; i <= 5; ++i) CHECK(rp26_produce(&manager));
    CHECK(!rp26_produce(&manager) && manager.count == 5);
    for (unsigned int i = 1; i <= 5; ++i) {
        CHECK(manager.buffer[manager.tail] == i);
        CHECK(rp26_consume(&manager));
    }
    CHECK(!rp26_consume(&manager));
    CHECK(rp26_produce(&manager) && manager.buffer[0] == 6);
    return 0;
}

typedef struct { uint8_t sent[512], echo[512]; size_t sent_size, echo_size; bool overflow; } Capture;
static void capture_send(void *context, const uint8_t *data, size_t size)
{
    Capture *cap = context;
    if (size > sizeof cap->sent - cap->sent_size) { cap->overflow = true; return; }
    memcpy(cap->sent + cap->sent_size, data, size);
    cap->sent_size += size;
}
static void capture_echo(void *context, const uint8_t *data, size_t size)
{
    Capture *cap = context;
    if (size > sizeof cap->echo - cap->echo_size) { cap->overflow = true; return; }
    memcpy(cap->echo + cap->echo_size, data, size);
    cap->echo_size += size;
}
static int test_chat(void)
{
    Capture cap = {0};
    rp33_Chat chat;
    CHECK(rp33_chat_init(&chat, capture_send, capture_echo, &cap));
    CHECK(rp33_chat_feed(&chat, (const uint8_t *)"hellp", 5));
    CHECK(cap.sent_size == 0 && chat.length == 5);
    CHECK(rp33_chat_feed(&chat, (const uint8_t *)"\b", 1));
    CHECK(chat.length == 4 && chat.buffer[4] == 0);
    CHECK(rp33_chat_feed(&chat, (const uint8_t *)"o\r", 2));
    CHECK(rp33_chat_feed(&chat, (const uint8_t *)"\n\r\n", 3));
    CHECK(cap.sent_size == 7 && memcmp(cap.sent, "hello\r\n", 7) == 0);
    CHECK(cap.echo_size == 11 && memcmp(cap.echo, "hellp\b \bo\r\n", 11) == 0);
    CHECK(chat.length == 0);
    CHECK(rp33_chat_feed(&chat, (const uint8_t *)"\b\x7f", 2));
    CHECK(chat.length == 0);
    uint8_t full[80];
    memset(full, 'x', sizeof full);
    CHECK(rp33_chat_feed(&chat, full, sizeof full));
    CHECK(chat.length == 63);
    CHECK(rp33_chat_feed(&chat, (const uint8_t *)"\r", 1));
    CHECK(cap.sent_size == 7 + 63 + 2 && !cap.overflow);
    CHECK(rp33_chat_feed(&chat, NULL, 0));
    CHECK(!rp33_chat_feed(&chat, NULL, 1));
    return 0;
}
int redpill_selftest(void)
{
    checks = 0;
    if (test_bits_memory() || test_containers() || test_streams() || test_chat())
        return 1;
    printf("\nPASS: %u boundary/reference checks\n", checks);
    return 0;
}
