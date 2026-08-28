#include "layer_packet.h"

#include <string.h>

uint16_t layer_packet_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    if (data == NULL) {
        return crc;
    }

    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8U;
        for (unsigned bit = 0; bit < 8U; ++bit) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }

    return crc;
}

layer_packet_status_t layer_packet_encode(
    const layer_packet_t *packet,
    uint8_t output[LAYER_PACKET_WIRE_SIZE])
{
    if (packet == NULL || output == NULL) {
        return LAYER_PACKET_INVALID_ARGUMENT;
    }
    if (packet->payload_length > LAYER_PACKET_PAYLOAD_CAPACITY) {
        return LAYER_PACKET_PAYLOAD_TOO_LONG;
    }

    memset(output, 0, LAYER_PACKET_WIRE_SIZE);
    output[0] = packet->version;
    output[1] = packet->type;
    output[2] = packet->ttl;
    output[3] = packet->sender;
    output[4] = packet->recipient;
    output[5] = (uint8_t)(packet->sequence & 0xFFU);
    output[6] = (uint8_t)(packet->sequence >> 8U);
    output[7] = packet->payload_length;
    memcpy(&output[8], packet->payload, packet->payload_length);

    uint16_t crc = layer_packet_crc16(output, 18U);
    output[18] = (uint8_t)(crc & 0xFFU);
    output[19] = (uint8_t)(crc >> 8U);
    return LAYER_PACKET_OK;
}

layer_packet_status_t layer_packet_decode(
    const uint8_t input[LAYER_PACKET_WIRE_SIZE],
    layer_packet_t *packet)
{
    if (input == NULL || packet == NULL) {
        return LAYER_PACKET_INVALID_ARGUMENT;
    }
    if (input[0] != LAYER_PACKET_VERSION) {
        return LAYER_PACKET_UNSUPPORTED_VERSION;
    }
    if (input[7] > LAYER_PACKET_PAYLOAD_CAPACITY) {
        return LAYER_PACKET_PAYLOAD_TOO_LONG;
    }

    uint16_t expected_crc =
        (uint16_t)input[18] | ((uint16_t)input[19] << 8U);
    if (layer_packet_crc16(input, 18U) != expected_crc) {
        return LAYER_PACKET_CRC_MISMATCH;
    }

    memset(packet, 0, sizeof(*packet));
    packet->version = input[0];
    packet->type = input[1];
    packet->ttl = input[2];
    packet->sender = input[3];
    packet->recipient = input[4];
    packet->sequence = (uint16_t)input[5] | ((uint16_t)input[6] << 8U);
    packet->payload_length = input[7];
    memcpy(packet->payload, &input[8], LAYER_PACKET_PAYLOAD_CAPACITY);
    return LAYER_PACKET_OK;
}

void layer_packet_dedup_init(layer_packet_dedup_t *dedup)
{
    if (dedup != NULL) {
        memset(dedup, 0, sizeof(*dedup));
    }
}

bool layer_packet_dedup_is_duplicate_or_record(
    layer_packet_dedup_t *dedup,
    uint8_t sender,
    uint16_t sequence)
{
    if (dedup == NULL) {
        return false;
    }

    for (size_t i = 0; i < dedup->count; ++i) {
        if (dedup->entries[i].sender == sender &&
            dedup->entries[i].sequence == sequence) {
            return true;
        }
    }

    size_t index;
    if (dedup->count < LAYER_PACKET_DEDUP_CAPACITY) {
        index = dedup->count;
        dedup->count++;
        dedup->next = dedup->count % LAYER_PACKET_DEDUP_CAPACITY;
    } else {
        index = dedup->next;
        dedup->next = (dedup->next + 1U) % LAYER_PACKET_DEDUP_CAPACITY;
    }

    dedup->entries[index].sender = sender;
    dedup->entries[index].sequence = sequence;
    return false;
}
