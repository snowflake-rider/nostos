#include "layer_packet.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_EQ_U16(expected, actual)                                      \
    do {                                                                    \
        uint16_t actual_value = (actual);                                   \
        if ((uint16_t)(expected) != actual_value) {                         \
            fprintf(stderr, "%s:%d expected 0x%04X, got 0x%04X\n",        \
                    __FILE__, __LINE__, (unsigned)(expected),               \
                    (unsigned)actual_value);                                \
            failures++;                                                     \
        }                                                                   \
    } while (0)

#define CHECK_TRUE(condition)                                                \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "%s:%d check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                        \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static void test_crc16_ccitt_false_standard_vector(void)
{
    static const uint8_t input[] = "123456789";

    CHECK_EQ_U16(0x29B1U, layer_packet_crc16(input, sizeof(input) - 1U));
}

static void test_encode_and_decode_hello_packet(void)
{
    static const uint8_t expected_wire[LAYER_PACKET_WIRE_SIZE] = {
        0x01U, 0x01U, 0x02U, 0x76U, 0xFFU, 0x01U, 0x00U, 0x05U,
        0x48U, 0x45U, 0x4CU, 0x4CU, 0x4FU, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0xA3U, 0x6DU,
    };
    layer_packet_t source = {
        .version = LAYER_PACKET_VERSION,
        .type = LAYER_PACKET_TYPE_HELLO,
        .ttl = 2U,
        .sender = 0x76U,
        .recipient = LAYER_PACKET_BROADCAST,
        .sequence = 1U,
        .payload_length = 5U,
        .payload = {'H', 'E', 'L', 'L', 'O'},
    };
    uint8_t wire[LAYER_PACKET_WIRE_SIZE] = {0};
    layer_packet_t decoded = {0};

    CHECK_TRUE(layer_packet_encode(&source, wire) == LAYER_PACKET_OK);
    CHECK_TRUE(memcmp(expected_wire, wire, sizeof(wire)) == 0);
    CHECK_TRUE(layer_packet_decode(wire, &decoded) == LAYER_PACKET_OK);
    CHECK_TRUE(decoded.version == source.version);
    CHECK_TRUE(decoded.type == source.type);
    CHECK_TRUE(decoded.ttl == source.ttl);
    CHECK_TRUE(decoded.sender == source.sender);
    CHECK_TRUE(decoded.recipient == source.recipient);
    CHECK_TRUE(decoded.sequence == source.sequence);
    CHECK_TRUE(decoded.payload_length == source.payload_length);
    CHECK_TRUE(memcmp(decoded.payload, source.payload,
                      LAYER_PACKET_PAYLOAD_CAPACITY) == 0);
}

static void test_decode_rejects_invalid_packets(void)
{
    uint8_t wire[LAYER_PACKET_WIRE_SIZE] = {
        0x01U, 0x01U, 0x02U, 0x76U, 0xFFU, 0x01U, 0x00U, 0x05U,
        0x48U, 0x45U, 0x4CU, 0x4CU, 0x4FU, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0xA3U, 0x6DU,
    };
    layer_packet_t decoded = {0};

    wire[8] ^= 0x01U;
    CHECK_TRUE(layer_packet_decode(wire, &decoded) ==
               LAYER_PACKET_CRC_MISMATCH);

    wire[8] ^= 0x01U;
    wire[0] = 2U;
    CHECK_TRUE(layer_packet_decode(wire, &decoded) ==
               LAYER_PACKET_UNSUPPORTED_VERSION);

    wire[0] = LAYER_PACKET_VERSION;
    wire[7] = LAYER_PACKET_PAYLOAD_CAPACITY + 1U;
    CHECK_TRUE(layer_packet_decode(wire, &decoded) ==
               LAYER_PACKET_PAYLOAD_TOO_LONG);
}

static void test_dedup_uses_sender_sequence_and_fifo_eviction(void)
{
    layer_packet_dedup_t dedup;

    layer_packet_dedup_init(&dedup);
    CHECK_TRUE(!layer_packet_dedup_is_duplicate_or_record(&dedup, 0x76U, 1U));
    CHECK_TRUE(layer_packet_dedup_is_duplicate_or_record(&dedup, 0x76U, 1U));
    CHECK_TRUE(!layer_packet_dedup_is_duplicate_or_record(&dedup, 0xB6U, 1U));

    layer_packet_dedup_init(&dedup);
    for (uint16_t sequence = 1U;
         sequence <= LAYER_PACKET_DEDUP_CAPACITY;
         ++sequence) {
        CHECK_TRUE(!layer_packet_dedup_is_duplicate_or_record(
            &dedup, 0x76U, sequence));
    }
    CHECK_TRUE(layer_packet_dedup_is_duplicate_or_record(&dedup, 0x76U, 1U));
    CHECK_TRUE(!layer_packet_dedup_is_duplicate_or_record(&dedup, 0x76U, 33U));
    CHECK_TRUE(!layer_packet_dedup_is_duplicate_or_record(&dedup, 0x76U, 1U));
}

int main(void)
{
    test_crc16_ccitt_false_standard_vector();
    test_encode_and_decode_hello_packet();
    test_decode_rejects_invalid_packets();
    test_dedup_uses_sender_sequence_and_fifo_eviction();

    if (failures != 0) {
        fprintf(stderr, "LAYER_PACKET_HOST_TESTS=FAIL failures=%d\n", failures);
        return 1;
    }

    puts("LAYER_PACKET_HOST_TESTS=PASS");
    return 0;
}
