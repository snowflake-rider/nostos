#include "layer_relay.h"

#include <string.h>

layer_relay_status_t layer_relay_prepare_forward(
    const layer_packet_t *received,
    layer_packet_t *forwarded)
{
    if (received == NULL || forwarded == NULL) {
        return LAYER_RELAY_INVALID_ARGUMENT;
    }
    if (received->ttl == 0U) {
        return LAYER_RELAY_TTL_EXHAUSTED;
    }

    *forwarded = *received;
    forwarded->ttl--;
    return LAYER_RELAY_OK;
}

layer_relay_path_t layer_relay_classify_path(uint8_t origin, uint8_t via)
{
    return origin == via ? LAYER_RELAY_PATH_DIRECT
                         : LAYER_RELAY_PATH_RELAYED;
}

void layer_path_dedup_init(layer_path_dedup_t *cache)
{
    if (cache != NULL) {
        memset(cache, 0, sizeof(*cache));
    }
}

bool layer_path_dedup_is_duplicate_or_record(
    layer_path_dedup_t *cache,
    uint8_t origin,
    uint16_t sequence,
    uint8_t via)
{
    if (cache == NULL) {
        return false;
    }

    for (size_t i = 0; i < cache->count; ++i) {
        const layer_path_identity_t *entry = &cache->entries[i];
        if (entry->origin == origin && entry->sequence == sequence &&
            entry->via == via) {
            return true;
        }
    }

    size_t index;
    if (cache->count < LAYER_PATH_DEDUP_CAPACITY) {
        index = cache->count;
        cache->count++;
        cache->next = cache->count % LAYER_PATH_DEDUP_CAPACITY;
    } else {
        index = cache->next;
        cache->next = (cache->next + 1U) % LAYER_PATH_DEDUP_CAPACITY;
    }

    cache->entries[index].origin = origin;
    cache->entries[index].sequence = sequence;
    cache->entries[index].via = via;
    return false;
}
