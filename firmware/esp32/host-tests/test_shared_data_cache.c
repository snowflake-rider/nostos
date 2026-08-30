#include "check.h"
#include "shared_data_cache.h"

#include <stdio.h>

static size_t encode_ride(uint8_t source, uint32_t session, uint16_t sequence,
                          uint16_t speed, uint32_t distance,
                          uint8_t wire[NOSTOS_WIRE_MAX])
{
    nostos_message_t message = {
        .type = NOSTOS_RIDE,
        .source_id = source,
        .session_id = session,
        .sequence = sequence,
        .payload.ride = {true, speed, distance},
    };
    size_t length = 0U;
    CHECK(nostos_message_encode(&message, wire, NOSTOS_WIRE_MAX, &length) ==
          NOSTOS_OK);
    return length;
}

static size_t encode_environment(uint8_t source, uint32_t session,
                                 uint16_t sequence,
                                 uint8_t wire[NOSTOS_WIRE_MAX])
{
    nostos_message_t message = {
        .type = NOSTOS_ENVIRONMENT,
        .source_id = source,
        .session_id = session,
        .sequence = sequence,
        .payload.environment = {
            .temperature_c_x10 = 250,
            .humidity_pct_x10 = 600U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        },
    };
    size_t length = 0U;
    CHECK(nostos_message_encode(&message, wire, NOSTOS_WIRE_MAX, &length) ==
          NOSTOS_OK);
    return length;
}

int main(void)
{
    shared_data_cache_t cache;
    shared_data_cache_init(&cache);
    CHECK(!cache.ride.occupied && !cache.environment.occupied);

    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = encode_ride(3U, 10U, 1U, 120U, 1000U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 10U) == NOSTOS_OK);
    CHECK(cache.ride.occupied && cache.ride.source_id == 3U);
    CHECK(shared_data_cache_store(&cache, wire, length, 11U) ==
          NOSTOS_DUPLICATE);
    CHECK(cache.ride.received_ms == 10U);
    length = encode_ride(3U, 10U, 0U, 110U, 900U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 12U) == NOSTOS_STALE);
    CHECK(cache.ride.sequence == 1U && cache.ride.received_ms == 10U);

    length = encode_environment(2U, 20U, 1U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 20U) == NOSTOS_OK);
    CHECK(cache.environment.occupied && cache.environment.source_id == 2U);
    CHECK(shared_data_cache_store(&cache, wire, length, 21U) ==
          NOSTOS_DUPLICATE);
    CHECK(cache.environment.received_ms == 20U);
    length = encode_environment(2U, 20U, 0U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 22U) == NOSTOS_STALE);
    CHECK(cache.environment.sequence == 1U &&
          cache.environment.received_ms == 20U);

    shared_data_cache_entry_t snapshots[2];
    size_t count = 0U;
    uint8_t served = shared_data_cache_copy_fresh(
        &cache, NOSTOS_SHARED_DATA_MASK, 30U, snapshots, &count);
    CHECK(served == NOSTOS_SHARED_DATA_MASK && count == 2U);
    CHECK(snapshots[0].wire[1] == NOSTOS_RIDE &&
          snapshots[0].source_id == 3U);
    CHECK(snapshots[1].wire[1] == NOSTOS_ENVIRONMENT &&
          snapshots[1].source_id == 2U);

    length = encode_ride(1U, 10U, 4U, 155U, 2000U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 100U) == NOSTOS_OK);
    CHECK(cache.ride.occupied && cache.ride.source_id == 1U);
    CHECK(cache.ride.session_id == 10U && cache.ride.sequence == 4U);
    CHECK(cache.ride.received_ms == 100U && cache.ride.length == length);

    CHECK(shared_data_cache_store(&cache, wire, length, 200U) ==
          NOSTOS_DUPLICATE);
    CHECK(cache.ride.received_ms == 100U);

    length = encode_ride(1U, 10U, 3U, 140U, 1500U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 201U) == NOSTOS_STALE);
    CHECK(cache.ride.sequence == 4U && cache.ride.received_ms == 100U);

    length = encode_ride(1U, 11U, 0U, 160U, 2500U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 300U) == NOSTOS_OK);
    CHECK(cache.ride.session_id == 11U && cache.ride.sequence == 0U);

    length = encode_environment(3U, 30U, 8U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 400U) == NOSTOS_OK);

    served = shared_data_cache_copy_fresh(
        &cache, NOSTOS_SHARED_DATA_MASK, 3300U, snapshots, &count);
    CHECK(served == NOSTOS_SHARED_DATA_MASK && count == 2U);
    CHECK(snapshots[0].wire[1] == NOSTOS_RIDE);
    CHECK(snapshots[1].wire[1] == NOSTOS_ENVIRONMENT);

    /* Freshness is inclusive, matching nostos_report_fresh(). */
    served = shared_data_cache_copy_fresh(
        &cache, NOSTOS_SHARED_DATA_RIDE,
        300U + NOSTOS_FRESH_MS, snapshots, &count);
    CHECK(served == NOSTOS_SHARED_DATA_RIDE && count == 1U);
    served = shared_data_cache_copy_fresh(
        &cache, NOSTOS_SHARED_DATA_RIDE,
        301U + NOSTOS_FRESH_MS, snapshots, &count);
    CHECK(served == 0U && count == 0U);

    /* The paired STM can recover the ESP writer's own-source sample because
     * endpoint init approved the current ESP session. */
    served = shared_data_cache_copy_fresh(
        &cache, NOSTOS_SHARED_DATA_MASK, 500U, snapshots, &count);
    CHECK(served == NOSTOS_SHARED_DATA_MASK && count == 2U);
    CHECK(snapshots[0].source_id == 1U);
    CHECK(snapshots[1].source_id == 3U);

    /* Session epoch is per source, not per sensor type. A newer ENV packet
     * invalidates old RIDE data, and that old session stays rejected even
     * after another source later replaces the occupied type slot. */
    shared_data_cache_init(&cache);
    length = encode_ride(2U, 50U, 9U, 180U, 5000U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 600U) == NOSTOS_OK);
    length = encode_environment(2U, 51U, 2U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 601U) == NOSTOS_OK);
    CHECK(!cache.ride.occupied);
    CHECK(cache.environment.occupied && cache.environment.session_id == 51U);

    length = encode_ride(2U, 50U, 10U, 181U, 5100U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 602U) == NOSTOS_STALE);
    length = encode_ride(2U, 51U, 5U, 182U, 5200U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 603U) == NOSTOS_OK);
    served = shared_data_cache_copy_fresh(
        &cache, NOSTOS_SHARED_DATA_MASK, 604U, snapshots, &count);
    CHECK(served == NOSTOS_SHARED_DATA_MASK && count == 2U);
    CHECK(snapshots[0].wire[1] == NOSTOS_ENVIRONMENT);
    CHECK(snapshots[0].sequence == 2U);
    CHECK(snapshots[1].wire[1] == NOSTOS_RIDE);
    CHECK(snapshots[1].sequence == 5U);

    length = encode_ride(2U, 52U, 0U, 183U, 5300U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 605U) == NOSTOS_OK);
    CHECK(!cache.environment.occupied);
    length = encode_ride(3U, 1U, 0U, 90U, 1000U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 606U) == NOSTOS_OK);
    length = encode_environment(2U, 51U, 9U, wire);
    CHECK(shared_data_cache_store(&cache, wire, length, 607U) == NOSTOS_STALE);

    shared_data_cache_init(&cache);
    CHECK(!cache.ride.occupied && !cache.environment.occupied);

    puts("PASS RAM cache source epoch, sequence order, replay, TTL, and reset");
    return 0;
}
