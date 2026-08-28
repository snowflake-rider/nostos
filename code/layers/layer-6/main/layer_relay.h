#ifndef LAYER_RELAY_H
#define LAYER_RELAY_H

#include "layer_packet.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LAYER_PATH_DEDUP_CAPACITY 32U

typedef enum {
    LAYER_RELAY_OK = 0,
    LAYER_RELAY_INVALID_ARGUMENT,
    LAYER_RELAY_TTL_EXHAUSTED,
} layer_relay_status_t;

typedef enum {
    LAYER_RELAY_PATH_DIRECT = 0,
    LAYER_RELAY_PATH_RELAYED,
} layer_relay_path_t;

typedef struct {
    uint8_t origin;
    uint16_t sequence;
    uint8_t via;
} layer_path_identity_t;

typedef struct {
    layer_path_identity_t entries[LAYER_PATH_DEDUP_CAPACITY];
    size_t count;
    size_t next;
} layer_path_dedup_t;

layer_relay_status_t layer_relay_prepare_forward(
    const layer_packet_t *received,
    layer_packet_t *forwarded);

layer_relay_path_t layer_relay_classify_path(uint8_t origin, uint8_t via);

void layer_path_dedup_init(layer_path_dedup_t *cache);

bool layer_path_dedup_is_duplicate_or_record(
    layer_path_dedup_t *cache,
    uint8_t origin,
    uint16_t sequence,
    uint8_t via);

#endif
