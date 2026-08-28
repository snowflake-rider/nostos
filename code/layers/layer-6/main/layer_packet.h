#ifndef LAYER_PACKET_H
#define LAYER_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LAYER_PACKET_WIRE_SIZE 20U
#define LAYER_PACKET_PAYLOAD_CAPACITY 10U
#define LAYER_PACKET_DEDUP_CAPACITY 32U

#define LAYER_PACKET_VERSION 1U
#define LAYER_PACKET_TYPE_HELLO 1U
#define LAYER_PACKET_BROADCAST 0xFFU

typedef enum {
    LAYER_PACKET_OK = 0,
    LAYER_PACKET_INVALID_ARGUMENT,
    LAYER_PACKET_PAYLOAD_TOO_LONG,
    LAYER_PACKET_UNSUPPORTED_VERSION,
    LAYER_PACKET_CRC_MISMATCH,
} layer_packet_status_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t ttl;
    uint8_t sender;
    uint8_t recipient;
    uint16_t sequence;
    uint8_t payload_length;
    uint8_t payload[LAYER_PACKET_PAYLOAD_CAPACITY];
} layer_packet_t;

typedef struct {
    uint8_t sender;
    uint16_t sequence;
} layer_packet_identity_t;

typedef struct {
    layer_packet_identity_t entries[LAYER_PACKET_DEDUP_CAPACITY];
    size_t count;
    size_t next;
} layer_packet_dedup_t;

uint16_t layer_packet_crc16(const uint8_t *data, size_t length);

layer_packet_status_t layer_packet_encode(
    const layer_packet_t *packet,
    uint8_t output[LAYER_PACKET_WIRE_SIZE]);

layer_packet_status_t layer_packet_decode(
    const uint8_t input[LAYER_PACKET_WIRE_SIZE],
    layer_packet_t *packet);

void layer_packet_dedup_init(layer_packet_dedup_t *dedup);

bool layer_packet_dedup_is_duplicate_or_record(
    layer_packet_dedup_t *dedup,
    uint8_t sender,
    uint16_t sequence);

#endif
