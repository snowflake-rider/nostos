#include "application_event_heap.h"

#include <limits.h>
#include <string.h>

static uint8_t event_priority(uint8_t type)
{
    if (type == NOSTOS_FALL || type == NOSTOS_FALL_CLEAR) return 1U;
    if (type == NOSTOS_STOP) return 2U;
    if (type == NOSTOS_SPEED_DOWN) return 3U;
    if (type == NOSTOS_SPEED_UP) return 4U;
    return 0U;
}

static bool entry_before(
    const application_event_heap_entry_t *left,
    const application_event_heap_entry_t *right)
{
    return left->priority < right->priority ||
        (left->priority == right->priority &&
         left->arrival_order < right->arrival_order);
}

static void swap_entries(
    application_event_heap_entry_t *left,
    application_event_heap_entry_t *right)
{
    application_event_heap_entry_t temporary = *left;
    *left = *right;
    *right = temporary;
}

static void sift_down(application_event_heap_t *heap, size_t index)
{
    for (;;) {
        size_t left = index * 2U + 1U;
        if (left >= heap->count) break;
        size_t right = left + 1U;
        size_t smallest = left;
        if (right < heap->count &&
            entry_before(&heap->entries[right], &heap->entries[left])) {
            smallest = right;
        }
        if (!entry_before(&heap->entries[smallest], &heap->entries[index])) {
            break;
        }
        swap_entries(&heap->entries[index], &heap->entries[smallest]);
        index = smallest;
    }
}

static void increment_class(application_event_heap_t *heap, uint8_t priority)
{
    if (priority == 1U) {
        ++heap->urgent_count;
    } else if (priority == 2U) {
        ++heap->stop_count;
    } else {
        ++heap->normal_count;
    }
}

static void decrement_class(application_event_heap_t *heap, uint8_t priority)
{
    if (priority == 1U) {
        --heap->urgent_count;
    } else if (priority == 2U) {
        --heap->stop_count;
    } else {
        --heap->normal_count;
    }
}

void application_event_heap_init(application_event_heap_t *heap)
{
    if (heap != NULL) {
        *heap = (application_event_heap_t){.next_arrival_order = 1U};
    }
}

nostos_result_t application_event_heap_push(
    application_event_heap_t *heap,
    const uint8_t *wire,
    size_t length,
    uint32_t received_ms)
{
    if (heap == NULL || wire == NULL) return NOSTOS_BAD_ARGUMENT;
    nostos_message_t message;
    nostos_result_t decoded = nostos_message_decode(wire, length, &message);
    if (decoded != NOSTOS_OK) return decoded;
    uint8_t priority = event_priority(message.type);
    if (priority == 0U) return NOSTOS_UNSUPPORTED_TYPE;
    if (heap->next_arrival_order == 0U ||
        heap->next_arrival_order == UINT64_MAX) {
        return NOSTOS_EXHAUSTED;
    }
    if (heap->count == APPLICATION_EVENT_HEAP_CAPACITY ||
        (priority > 1U &&
         heap->stop_count + heap->normal_count ==
            APPLICATION_EVENT_HEAP_NONURGENT_CAPACITY) ||
        (priority > 2U &&
         heap->normal_count == APPLICATION_EVENT_HEAP_NORMAL_CAPACITY)) {
        return NOSTOS_FULL;
    }

    size_t index = heap->count++;
    application_event_heap_entry_t entry = {
        .job = {
            .length = length,
            .received_ms = received_ms,
            .direction = NOSTOS_TO_UART,
        },
        .arrival_order = heap->next_arrival_order++,
        .priority = priority,
    };
    memcpy(entry.job.wire, wire, length);
    heap->entries[index] = entry;
    increment_class(heap, priority);

    while (index != 0U) {
        size_t parent = (index - 1U) / 2U;
        if (!entry_before(&heap->entries[index], &heap->entries[parent])) break;
        swap_entries(&heap->entries[index], &heap->entries[parent]);
        index = parent;
    }
    return NOSTOS_OK;
}

static application_event_heap_entry_t remove_min(
    application_event_heap_t *heap)
{
    application_event_heap_entry_t minimum = heap->entries[0];
    --heap->count;
    decrement_class(heap, minimum.priority);
    if (heap->count == 0U) return minimum;

    heap->entries[0] = heap->entries[heap->count];
    sift_down(heap, 0U);
    return minimum;
}

nostos_result_t application_event_heap_pop(
    application_event_heap_t *heap,
    uint32_t now_ms,
    nostos_job_t *job)
{
    if (heap == NULL || job == NULL) return NOSTOS_BAD_ARGUMENT;
    while (heap->count != 0U) {
        application_event_heap_entry_t minimum = remove_min(heap);
        if (minimum.priority != 1U &&
            (uint32_t)(now_ms - minimum.job.received_ms) >
                NOSTOS_BRIDGE_MAX_AGE_MS) {
            continue;
        }
        *job = minimum.job;
        return NOSTOS_OK;
    }
    return NOSTOS_EMPTY;
}

uint8_t application_event_heap_priority(
    const application_event_heap_t *heap)
{
    return heap != NULL && heap->count != 0U
        ? heap->entries[0].priority : 0U;
}

size_t application_event_heap_discard_source_before_session(
    application_event_heap_t *heap,
    uint8_t source_id,
    uint32_t session_id)
{
    if (heap == NULL || source_id < 1U || source_id > NOSTOS_NODE_COUNT ||
        session_id == 0U) {
        return 0U;
    }
    size_t write_index = 0U;
    size_t removed = 0U;
    for (size_t read_index = 0U; read_index < heap->count; ++read_index) {
        nostos_message_t message;
        nostos_result_t decoded = nostos_message_decode(
            heap->entries[read_index].job.wire,
            heap->entries[read_index].job.length, &message);
        bool discard = decoded != NOSTOS_OK ||
            (message.source_id == source_id &&
             message.session_id < session_id);
        if (discard) {
            ++removed;
            continue;
        }
        if (write_index != read_index) {
            heap->entries[write_index] = heap->entries[read_index];
        }
        ++write_index;
    }
    heap->count = write_index;
    heap->urgent_count = 0U;
    heap->stop_count = 0U;
    heap->normal_count = 0U;
    for (size_t i = 0U; i < heap->count; ++i) {
        increment_class(heap, heap->entries[i].priority);
    }
    for (size_t i = heap->count / 2U; i != 0U; --i) {
        sift_down(heap, i - 1U);
    }
    return removed;
}
