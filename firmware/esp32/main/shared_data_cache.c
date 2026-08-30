#include "shared_data_cache.h"

#include <string.h>

static shared_data_cache_entry_t *entry_for_type(
    shared_data_cache_t *cache,
    uint8_t type)
{
    if (type == NOSTOS_RIDE) return &cache->ride;
    if (type == NOSTOS_ENVIRONMENT) return &cache->environment;
    return NULL;
}

static void invalidate_older_source_session(
    shared_data_cache_entry_t *entry,
    uint8_t source_id,
    uint32_t session_id)
{
    if (entry->occupied && entry->source_id == source_id &&
        entry->session_id < session_id) {
        *entry = (shared_data_cache_entry_t){0};
    }
}

static bool entry_is_fresh(
    const shared_data_cache_entry_t *entry,
    uint32_t now_ms)
{
    return entry->occupied &&
        (uint32_t)(now_ms - entry->received_ms) <= NOSTOS_FRESH_MS;
}

void shared_data_cache_init(shared_data_cache_t *cache)
{
    if (cache != NULL) *cache = (shared_data_cache_t){0};
}

nostos_result_t shared_data_cache_store(
    shared_data_cache_t *cache,
    const uint8_t *wire,
    size_t length,
    uint32_t received_ms)
{
    if (cache == NULL || wire == NULL) return NOSTOS_BAD_ARGUMENT;

    nostos_message_t message;
    nostos_result_t result = nostos_message_decode(wire, length, &message);
    if (result != NOSTOS_OK) return result;
    shared_data_cache_entry_t *entry = entry_for_type(cache, message.type);
    if (entry == NULL) return NOSTOS_UNSUPPORTED_TYPE;

    size_t source_index = (size_t)(message.source_id - 1U);
    if (cache->session_seen[source_index] &&
        message.session_id < cache->latest_session[source_index]) {
        return NOSTOS_STALE;
    }
    if (!cache->session_seen[source_index] ||
        message.session_id > cache->latest_session[source_index]) {
        cache->latest_session[source_index] = message.session_id;
        cache->session_seen[source_index] = true;
        invalidate_older_source_session(
            &cache->ride, message.source_id, message.session_id);
        invalidate_older_source_session(
            &cache->environment, message.source_id, message.session_id);
    }

    if (entry->occupied && entry->source_id == message.source_id) {
        if (message.session_id < entry->session_id ||
            (message.session_id == entry->session_id &&
             message.sequence < entry->sequence)) {
            return NOSTOS_STALE;
        }
        if (message.session_id == entry->session_id &&
            message.sequence == entry->sequence) {
            return NOSTOS_DUPLICATE;
        }
    }

    shared_data_cache_entry_t replacement = {
        .length = length,
        .source_id = message.source_id,
        .session_id = message.session_id,
        .sequence = message.sequence,
        .received_ms = received_ms,
        .occupied = true,
    };
    memcpy(replacement.wire, wire, length);
    *entry = replacement;
    return NOSTOS_OK;
}

uint8_t shared_data_cache_copy_fresh(
    const shared_data_cache_t *cache,
    uint8_t requested_mask,
    uint32_t now_ms,
    shared_data_cache_entry_t snapshots[2],
    size_t *snapshot_count)
{
    if (snapshot_count != NULL) *snapshot_count = 0U;
    if (cache == NULL || snapshots == NULL || snapshot_count == NULL ||
        requested_mask == 0U ||
        (requested_mask & (uint8_t)~NOSTOS_SHARED_DATA_MASK) != 0U) {
        return 0U;
    }

    uint8_t copied_mask = 0U;
    if ((requested_mask & NOSTOS_SHARED_DATA_RIDE) != 0U &&
        entry_is_fresh(&cache->ride, now_ms)) {
        snapshots[*snapshot_count] = cache->ride;
        ++*snapshot_count;
        copied_mask |= NOSTOS_SHARED_DATA_RIDE;
    }
    if ((requested_mask & NOSTOS_SHARED_DATA_ENVIRONMENT) != 0U &&
        entry_is_fresh(&cache->environment, now_ms)) {
        snapshots[*snapshot_count] = cache->environment;
        ++*snapshot_count;
        copied_mask |= NOSTOS_SHARED_DATA_ENVIRONMENT;
    }
    if (*snapshot_count == 2U &&
        snapshots[0].source_id == snapshots[1].source_id &&
        snapshots[0].session_id == snapshots[1].session_id &&
        snapshots[0].sequence > snapshots[1].sequence) {
        shared_data_cache_entry_t first = snapshots[0];
        snapshots[0] = snapshots[1];
        snapshots[1] = first;
    }
    return copied_mask;
}
