#include "layer_relay.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_TRUE(condition)                                                \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "%s:%d check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                        \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static layer_packet_t make_packet(uint8_t ttl)
{
    const layer_packet_t packet = {
        .version = LAYER_PACKET_VERSION,
        .type = LAYER_PACKET_TYPE_HELLO,
        .ttl = ttl,
        .sender = 0x76U,
        .recipient = LAYER_PACKET_BROADCAST,
        .sequence = 0x1234U,
        .payload_length = 5U,
        .payload = {'H', 'E', 'L', 'L', 'O'},
    };
    return packet;
}

static void test_forward_decrements_only_ttl_and_regenerates_crc(void)
{
    const layer_packet_t received = make_packet(2U);
    layer_packet_t forwarded = {0};
    uint8_t received_wire[LAYER_PACKET_WIRE_SIZE] = {0};
    uint8_t forwarded_wire[LAYER_PACKET_WIRE_SIZE] = {0};
    layer_packet_t decoded = {0};

    CHECK_TRUE(layer_relay_prepare_forward(&received, &forwarded) ==
               LAYER_RELAY_OK);
    CHECK_TRUE(forwarded.ttl == 1U);
    CHECK_TRUE(forwarded.version == received.version);
    CHECK_TRUE(forwarded.type == received.type);
    CHECK_TRUE(forwarded.sender == received.sender);
    CHECK_TRUE(forwarded.recipient == received.recipient);
    CHECK_TRUE(forwarded.sequence == received.sequence);
    CHECK_TRUE(forwarded.payload_length == received.payload_length);
    CHECK_TRUE(memcmp(forwarded.payload, received.payload,
                      LAYER_PACKET_PAYLOAD_CAPACITY) == 0);

    CHECK_TRUE(layer_packet_encode(&received, received_wire) == LAYER_PACKET_OK);
    CHECK_TRUE(layer_packet_encode(&forwarded, forwarded_wire) == LAYER_PACKET_OK);
    CHECK_TRUE(memcmp(&received_wire[18], &forwarded_wire[18], 2U) != 0);
    CHECK_TRUE(layer_packet_decode(forwarded_wire, &decoded) == LAYER_PACKET_OK);
    CHECK_TRUE(decoded.ttl == 1U);
}

static void test_ttl_one_forwards_to_zero_and_zero_is_exhausted(void)
{
    const layer_packet_t ttl_one = make_packet(1U);
    const layer_packet_t ttl_zero = make_packet(0U);
    layer_packet_t forwarded = {0};

    CHECK_TRUE(layer_relay_prepare_forward(&ttl_one, &forwarded) ==
               LAYER_RELAY_OK);
    CHECK_TRUE(forwarded.ttl == 0U);
    CHECK_TRUE(layer_relay_prepare_forward(&ttl_zero, &forwarded) ==
               LAYER_RELAY_TTL_EXHAUSTED);
    CHECK_TRUE(layer_relay_prepare_forward(NULL, &forwarded) ==
               LAYER_RELAY_INVALID_ARGUMENT);
    CHECK_TRUE(layer_relay_prepare_forward(&ttl_one, NULL) ==
               LAYER_RELAY_INVALID_ARGUMENT);
}

static void test_path_classification_uses_origin_and_immediate_transmitter(void)
{
    CHECK_TRUE(layer_relay_classify_path(0x76U, 0x76U) ==
               LAYER_RELAY_PATH_DIRECT);
    CHECK_TRUE(layer_relay_classify_path(0x76U, 0xB6U) ==
               LAYER_RELAY_PATH_RELAYED);
}

static void test_path_dedup_includes_via_and_uses_fifo_eviction(void)
{
    layer_path_dedup_t cache;

    layer_path_dedup_init(&cache);
    CHECK_TRUE(!layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 1U, 0x76U));
    CHECK_TRUE(layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 1U, 0x76U));
    CHECK_TRUE(!layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 1U, 0xB6U));
    CHECK_TRUE(layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 1U, 0xB6U));

    layer_path_dedup_init(&cache);
    for (uint16_t sequence = 1U;
         sequence <= LAYER_PATH_DEDUP_CAPACITY;
         ++sequence) {
        CHECK_TRUE(!layer_path_dedup_is_duplicate_or_record(
            &cache, 0x76U, sequence, 0xB6U));
    }
    CHECK_TRUE(layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 1U, 0xB6U));
    CHECK_TRUE(!layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 33U, 0xB6U));
    CHECK_TRUE(!layer_path_dedup_is_duplicate_or_record(
        &cache, 0x76U, 1U, 0xB6U));
}

int main(void)
{
    test_forward_decrements_only_ttl_and_regenerates_crc();
    test_ttl_one_forwards_to_zero_and_zero_is_exhausted();
    test_path_classification_uses_origin_and_immediate_transmitter();
    test_path_dedup_includes_via_and_uses_fifo_eviction();

    if (failures != 0) {
        fprintf(stderr, "LAYER_RELAY_HOST_TESTS=FAIL failures=%d\n", failures);
        return 1;
    }

    puts("LAYER_RELAY_HOST_TESTS=PASS");
    return 0;
}
