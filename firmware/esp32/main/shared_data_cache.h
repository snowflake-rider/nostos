#ifndef SHARED_DATA_CACHE_H
#define SHARED_DATA_CACHE_H

#include "nostos_state.h"

typedef struct {
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length;
    uint8_t source_id;
    uint32_t session_id;
    uint16_t sequence;
    uint32_t received_ms;
    bool occupied;
} shared_data_cache_entry_t;

typedef struct {
    shared_data_cache_entry_t ride;
    shared_data_cache_entry_t environment;
    uint32_t latest_session[NOSTOS_NODE_COUNT];
    bool session_seen[NOSTOS_NODE_COUNT];
} shared_data_cache_t;

/* The cache is RAM-only fixed storage. Its caller provides serialization. */
void shared_data_cache_init(shared_data_cache_t *cache);

/* Stores RIDE/ENVIRONMENT from any valid source. A replay never refreshes TTL. */
nostos_result_t shared_data_cache_store(shared_data_cache_t *cache,
    const uint8_t *wire, size_t length, uint32_t received_ms);

/* Copies fresh entries for requested groups. Local-source entries are valid:
 * the paired STM approves the ESP writer session during endpoint init. */
uint8_t shared_data_cache_copy_fresh(const shared_data_cache_t *cache,
    uint8_t requested_mask, uint32_t now_ms,
    shared_data_cache_entry_t snapshots[2], size_t *snapshot_count);

#endif
