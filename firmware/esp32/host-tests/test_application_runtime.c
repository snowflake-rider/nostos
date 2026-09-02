#include "application_runtime.h"
#include "check.h"

#include <stdio.h>

static nostos_message_t ride_state(
    uint8_t source,
    bool sensor_valid,
    uint16_t speed_x10_kmh,
    uint32_t trip_distance_m)
{
    nostos_message_t message;
    CHECK(nostos_message_make_ride(
        &message, source, sensor_valid, speed_x10_kmh,
        trip_distance_m) == NOSTOS_OK);
    return message;
}

static nostos_message_t environment_state(
    uint8_t source,
    bool sensor_valid,
    int16_t temperature_x10_c,
    uint16_t humidity_x10_pct)
{
    nostos_message_t message;
    CHECK(nostos_message_make_environment(
        &message, source, sensor_valid, temperature_x10_c,
        humidity_x10_pct) == NOSTOS_OK);
    return message;
}

static nostos_message_t local_stop(
    uint32_t local_request_id,
    nostos_stop_reason_t reason)
{
    nostos_message_t message;
    CHECK(nostos_message_make_stop(
        &message, NOSTOS_LOCAL_SOURCE_NODE_ID, local_request_id,
        (uint8_t)reason) == NOSTOS_OK);
    return message;
}

static nostos_message_t remote_stop(
    uint8_t source_node_id,
    uint32_t request_id,
    nostos_stop_reason_t reason)
{
    nostos_message_t message;
    CHECK(nostos_message_make_stop(
        &message, source_node_id, request_id,
        (uint8_t)reason) == NOSTOS_OK);
    return message;
}

static nostos_message_t local_stop_ack(uint32_t request_id)
{
    nostos_message_t message;
    CHECK(nostos_message_make_stop_ack(
        &message, NOSTOS_LOCAL_SOURCE_NODE_ID, request_id) == NOSTOS_OK);
    return message;
}

static void test_state_binding_freshness_and_repeat(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x0007U, 1U, 2U) == NOSTOS_OK);

    nostos_message_t remote = environment_state(2U, false, 245, 500U);
    CHECK(application_runtime_accept_mesh_state(
        &runtime, &remote, 3U, 100U) == NOSTOS_BAD_VALUE);
    CHECK(application_runtime_accept_mesh_state(
        &runtime, &remote, 2U, 100U) == NOSTOS_OK);
    remote.source_node_id = 3U;
    CHECK(application_runtime_accept_mesh_state(
        &runtime, &remote, 3U, 101U) == NOSTOS_BAD_VALUE);
    remote.source_node_id = 2U;

    nostos_message_t cached;
    bool sensor_valid = true;
    CHECK(application_runtime_state(
        &runtime, NOSTOS_TOPIC_ENVIRONMENT, 2U, 6100U, &cached,
        &sensor_valid) == APPLICATION_STATE_FRESH);
    CHECK(!sensor_valid);
    CHECK(application_runtime_state(
        &runtime, NOSTOS_TOPIC_ENVIRONMENT, 2U, 6101U, &cached,
        &sensor_valid) == APPLICATION_STATE_STALE);
    CHECK(!sensor_valid);
    CHECK(application_runtime_state(
        &runtime, NOSTOS_TOPIC_ENVIRONMENT, 2U, 20100U, &cached,
        &sensor_valid) == APPLICATION_STATE_STALE);
    CHECK(application_runtime_state(
        &runtime, NOSTOS_TOPIC_ENVIRONMENT, 2U, 20101U, &cached,
        &sensor_valid) == APPLICATION_STATE_UNKNOWN);

    nostos_message_t local = ride_state(1U, true, 200U, 55U);
    CHECK(application_runtime_publish_state(
        &runtime, &local, 1000U) == NOSTOS_OK);
    CHECK(application_runtime_take_due_state(
        &runtime, 1000U, &cached) == NOSTOS_OK);
    CHECK(cached.source_node_id == 1U);
    CHECK(application_runtime_take_due_state(
        &runtime, 2999U, &cached) == NOSTOS_EMPTY);
    CHECK(application_runtime_take_due_state(
        &runtime, 3000U, &cached) == NOSTOS_OK);
}

static void test_local_source_stamping(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 4U, 0x001FU, 1U, 2U) == NOSTOS_OK);
    nostos_message_t pace;
    CHECK(nostos_message_make_pace(
        &pace, NOSTOS_LOCAL_SOURCE_NODE_ID, 123U,
        NOSTOS_PACE_ACCELERATE) == NOSTOS_OK);
    CHECK(application_runtime_stamp_local(&runtime, &pace) == NOSTOS_OK);
    CHECK(pace.source_node_id == 4U);
    uint8_t wire[NOSTOS_APPLICATION_MAX_SIZE];
    size_t length = 0U;
    CHECK(nostos_message_encode(
        &pace, wire, sizeof(wire), &length) == NOSTOS_OK);

    pace.source_node_id = 3U;
    CHECK(application_runtime_stamp_local(
        &runtime, &pace) == NOSTOS_BAD_VALUE);
}

static void test_state_latest_value_replaces_older_value(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x0007U, 1U, 2U) == NOSTOS_OK);

    nostos_message_t first = ride_state(1U, true, 250U, 100U);
    nostos_message_t latest = ride_state(1U, false, 999U, 999U);
    CHECK(application_runtime_publish_state(
        &runtime, &first, 10U) == NOSTOS_OK);
    CHECK(application_runtime_publish_state(
        &runtime, &latest, 11U) == NOSTOS_OK);

    nostos_message_t observed;
    bool sensor_valid = true;
    CHECK(application_runtime_state(
        &runtime, NOSTOS_TOPIC_RIDE, 1U, 11U,
        &observed, &sensor_valid) == APPLICATION_STATE_FRESH);
    CHECK(!sensor_valid);
    CHECK(observed.payload.state_update.value.ride.speed_x10_kmh == 0U);
    CHECK(observed.payload.state_update.value.ride.trip_distance_m == 0U);
    CHECK(application_runtime_take_due_state(
        &runtime, 11U, &observed) == NOSTOS_OK);
    CHECK(!observed.payload.state_update.sensor_valid);

    first = environment_state(2U, false, 0, 0U);
    latest = environment_state(2U, true, 321, 654U);
    CHECK(application_runtime_accept_mesh_state(
        &runtime, &first, 2U, 20U) == NOSTOS_OK);
    CHECK(application_runtime_accept_mesh_state(
        &runtime, &latest, 2U, 21U) == NOSTOS_OK);
    CHECK(application_runtime_state(
        &runtime, NOSTOS_TOPIC_ENVIRONMENT, 2U, 21U,
        &observed, &sensor_valid) == APPLICATION_STATE_FRESH);
    CHECK(sensor_valid);
    CHECK(observed.payload.state_update.value.environment.temperature_x10_c ==
        321);
    CHECK(observed.payload.state_update.value.environment.humidity_x10_pct ==
        654U);
}

static void test_ten_node_capacity(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 10U, 0x03FFU, 10U, 9U) == NOSTOS_OK);
    nostos_message_t ride = ride_state(10U, true, 123U, 456U);
    CHECK(application_runtime_publish_state(
        &runtime, &ride, 0U) == NOSTOS_OK);
    nostos_message_t stop;
    CHECK(nostos_message_make_stop(
        &stop, 10U, 99U, NOSTOS_STOP_REASON_BUTTON) == NOSTOS_OK);
    CHECK(application_runtime_begin_stop_message(
        &runtime, &stop, 0U) == NOSTOS_OK);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x01FFU);
}

static void test_stop_group_ack_partial_retry_and_dedupe(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x0007U, 1U, 2U) == NOSTOS_OK);
    nostos_message_t request;
    CHECK(nostos_message_make_stop(
        &request, 1U, UINT32_C(0xA55A1234),
        NOSTOS_STOP_REASON_BUTTON) == NOSTOS_OK);
    CHECK(application_runtime_begin_stop_message(
        &runtime, &request, 100U) == NOSTOS_OK);
    CHECK(request.type == NOSTOS_MESSAGE_STOP_REQUEST);
    CHECK(request.source_node_id == 1U);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x0006U);

    nostos_message_t ack;
    CHECK(nostos_message_make_stop_ack(
        &ack, 2U, request.payload.stop_request.request_id) == NOSTOS_OK);
    CHECK(application_runtime_accept_stop_ack(
        &runtime, &ack, 3U) == NOSTOS_BAD_VALUE);
    CHECK(application_runtime_accept_stop_ack(
        &runtime, &ack, 2U) == NOSTOS_OK);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x0004U);

    uint16_t missing = 0U;
    nostos_message_t retry;
    CHECK(application_runtime_stop_retry_due(
        &runtime, 1099U, &missing, &retry) == NOSTOS_EMPTY);
    CHECK(application_runtime_stop_retry_due(
        &runtime, 1100U, &missing, &retry) == NOSTOS_OK);
    CHECK(missing == 0x0004U);
    CHECK(retry.payload.stop_request.request_id ==
        request.payload.stop_request.request_id);

    /* A temporary Mesh configuration outage may skip several retry periods.
     * The retained transaction must become due immediately when transport
     * resumes, without changing its request identity or missing-peer mask. */
    CHECK(application_runtime_stop_retry_due(
        &runtime, 60000U, &missing, &retry) == NOSTOS_OK);
    CHECK(missing == 0x0004U);
    CHECK(retry.payload.stop_request.request_id ==
        request.payload.stop_request.request_id);

    bool duplicate = false;
    nostos_message_t incoming;
    CHECK(nostos_message_make_stop(
        &incoming, 3U, 77U, NOSTOS_STOP_REASON_FALL) == NOSTOS_OK);
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &incoming, 3U, &duplicate) == NOSTOS_OK);
    CHECK(!duplicate);
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &incoming, 3U, &duplicate) == NOSTOS_OK);
    CHECK(!duplicate); /* A check alone does not commit acceptance. */
    CHECK(application_runtime_remember_request(
        &runtime, &incoming, 3U) == NOSTOS_OK);
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &incoming, 3U, &duplicate) == NOSTOS_OK);
    CHECK(duplicate);

    nostos_message_t pace;
    CHECK(nostos_message_make_pace(
        &pace, 3U, 77U, NOSTOS_PACE_ACCELERATE) == NOSTOS_OK);
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &pace, 3U, &duplicate) == NOSTOS_OK);
    CHECK(!duplicate);
    CHECK(application_runtime_remember_request(
        &runtime, &pace, 3U) == NOSTOS_OK);
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &pace, 3U, &duplicate) == NOSTOS_OK);
    CHECK(duplicate);
    /* Same source/id is independent across PACE and STOP message types. */
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &incoming, 3U, &duplicate) == NOSTOS_OK);
    CHECK(duplicate);
    incoming.source_node_id = 2U;
    CHECK(application_runtime_request_is_duplicate(
        &runtime, &incoming, 3U, &duplicate) == NOSTOS_BAD_VALUE);

    CHECK(nostos_message_make_stop_ack(
        &ack, 3U, request.payload.stop_request.request_id) == NOSTOS_OK);
    CHECK(application_runtime_accept_stop_ack(
        &runtime, &ack, 3U) == NOSTOS_OK);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0U);
    CHECK(application_runtime_stop_retry_due(
        &runtime, 2100U, &missing, &retry) == NOSTOS_EMPTY);
}

static void test_local_stop_mapping_replay_and_expiry(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x0007U, 1U, 2U) == NOSTOS_OK);
    nostos_message_t local = local_stop(11U, NOSTOS_STOP_REASON_BUTTON);
    nostos_message_t mesh;
    application_local_stop_action_t action;

    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 1001U, 100U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);
    CHECK(mesh.source_node_id == 1U);
    CHECK(mesh.payload.stop_request.request_id == 1001U);
    CHECK(mesh.payload.stop_request.reason == NOSTOS_STOP_REASON_BUTTON);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x0006U);

    /* Completion does not erase the short local replay mapping. */
    nostos_message_t ack;
    CHECK(nostos_message_make_stop_ack(&ack, 2U, 1001U) == NOSTOS_OK);
    CHECK(application_runtime_accept_stop_ack(
        &runtime, &ack, 2U) == NOSTOS_OK);
    CHECK(nostos_message_make_stop_ack(&ack, 3U, 1001U) == NOSTOS_OK);
    CHECK(application_runtime_accept_stop_ack(
        &runtime, &ack, 3U) == NOSTOS_OK);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0U);

    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 1002U, 2099U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_ACK_ONLY);
    CHECK(mesh.payload.stop_request.request_id == 1001U);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0U);

    /* The exact deadline is expired: duplicate STOP is safer than suppression. */
    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 1003U, 2100U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);
    CHECK(mesh.payload.stop_request.request_id == 1003U);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x0006U);
}

static void test_local_stop_fall_priority_and_new_identity(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x0007U, 1U, 2U) == NOSTOS_OK);
    nostos_message_t button = local_stop(21U, NOSTOS_STOP_REASON_BUTTON);
    nostos_message_t fall = local_stop(21U, NOSTOS_STOP_REASON_FALL);
    nostos_message_t mesh;
    application_local_stop_action_t action;

    CHECK(application_runtime_accept_local_stop(
        &runtime, &button, 2001U, 0U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);

    /* Same local id is still a new transaction when FALL promotes BUTTON. */
    CHECK(application_runtime_accept_local_stop(
        &runtime, &fall, 2002U, 1U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);
    CHECK(mesh.payload.stop_request.request_id == 2002U);
    CHECK(mesh.payload.stop_request.reason == NOSTOS_STOP_REASON_FALL);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x0006U);

    nostos_message_t old_ack;
    CHECK(nostos_message_make_stop_ack(&old_ack, 2U, 2001U) == NOSTOS_OK);
    CHECK(application_runtime_accept_stop_ack(
        &runtime, &old_ack, 2U) == NOSTOS_EMPTY);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0x0006U);

    /* A replayed BUTTON cannot downgrade the accepted FALL. */
    CHECK(application_runtime_accept_local_stop(
        &runtime, &button, 2003U, 2U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_ACK_ONLY);
    CHECK(mesh.payload.stop_request.request_id == 2002U);
    CHECK(mesh.payload.stop_request.reason == NOSTOS_STOP_REASON_FALL);

    /* A different local identity always starts a new transaction. */
    button = local_stop(22U, NOSTOS_STOP_REASON_BUTTON);
    CHECK(application_runtime_accept_local_stop(
        &runtime, &button, 2004U, 3U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);
    CHECK(mesh.payload.stop_request.request_id == 2004U);
    CHECK(mesh.payload.stop_request.reason == NOSTOS_STOP_REASON_BUTTON);
}

static void test_local_stop_rejects_zero_mesh_id_and_wraps_time(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 10U, 0x03FFU, 10U, 9U) == NOSTOS_OK);
    nostos_message_t local = local_stop(31U, NOSTOS_STOP_REASON_FALL);
    nostos_message_t mesh;
    application_local_stop_action_t action;

    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 0U, 0U, &action, &mesh) == NOSTOS_BAD_VALUE);
    CHECK(application_runtime_stop_pending_mask(&runtime) == 0U);

    const uint32_t start = UINT32_MAX - 1000U;
    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 3001U, start, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);
    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 3002U, 998U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_ACK_ONLY);
    CHECK(mesh.payload.stop_request.request_id == 3001U);
    CHECK(application_runtime_accept_local_stop(
        &runtime, &local, 3003U, 999U, &action, &mesh) == NOSTOS_OK);
    CHECK(action == APPLICATION_LOCAL_STOP_NEW_MESH);
    CHECK(mesh.payload.stop_request.request_id == 3003U);
}

static void test_remote_stop_waits_for_local_ack_and_recovers_retries(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x0007U, 1U, 2U) == NOSTOS_OK);
    nostos_message_t request = remote_stop(
        2U, 4001U, NOSTOS_STOP_REASON_BUTTON);
    application_remote_stop_action_t action;

    CHECK(application_runtime_accept_remote_stop(
        &runtime, &request, 2U, &action) == NOSTOS_OK);
    CHECK(action == APPLICATION_REMOTE_STOP_FORWARD_UART);
    /* A Mesh retry before STM acceptance must be forwarded again. */
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &request, 2U, &action) == NOSTOS_OK);
    CHECK(action == APPLICATION_REMOTE_STOP_FORWARD_UART);

    nostos_message_t ack = local_stop_ack(4001U);
    uint8_t remote_source = 0U;
    CHECK(application_runtime_complete_remote_stop(
        &runtime, &ack, &remote_source) == NOSTOS_OK);
    CHECK(remote_source == 2U);

    /* If the outgoing Mesh ACK was lost, retry is ACKed from completed RAM
     * without delivering the same STOP to STM32 a third time. */
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &request, 2U, &action) == NOSTOS_OK);
    CHECK(action == APPLICATION_REMOTE_STOP_ACK_COMPLETED);
    CHECK(application_runtime_complete_remote_stop(
        &runtime, &ack, &remote_source) == NOSTOS_EMPTY);
}

static void test_remote_stop_rejects_ambiguous_source0_ack_ids(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x03FFU, 1U, 2U) == NOSTOS_OK);
    nostos_message_t source2 = remote_stop(
        2U, 5001U, NOSTOS_STOP_REASON_BUTTON);
    nostos_message_t source3 = remote_stop(
        3U, 5001U, NOSTOS_STOP_REASON_FALL);
    application_remote_stop_action_t action;
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &source2, 2U, &action) == NOSTOS_OK);
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &source3, 3U, &action) == NOSTOS_BAD_VALUE);

    nostos_message_t changed_reason = remote_stop(
        2U, 5001U, NOSTOS_STOP_REASON_FALL);
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &changed_reason, 2U, &action) == NOSTOS_BAD_VALUE);

    nostos_message_t ack = local_stop_ack(5001U);
    uint8_t remote_source = 0U;
    CHECK(application_runtime_complete_remote_stop(
        &runtime, &ack, &remote_source) == NOSTOS_OK);
    CHECK(remote_source == 2U);
    /* Retained completed ownership also prevents a late source-2 local ACK
     * from being mistaken for a new source-3 transaction with the same id. */
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &source3, 3U, &action) == NOSTOS_BAD_VALUE);
}

static void test_remote_stop_pending_capacity_is_bounded(void)
{
    application_runtime_t runtime;
    CHECK(application_runtime_init(
        &runtime, 1U, 0x03FFU, 1U, 2U) == NOSTOS_OK);
    application_remote_stop_action_t action;
    for (uint32_t i = 0U;
         i < APPLICATION_RUNTIME_REMOTE_STOP_CAPACITY; ++i) {
        uint8_t source = (uint8_t)(2U + (i % 9U));
        nostos_message_t request = remote_stop(
            source, 6001U + i, NOSTOS_STOP_REASON_BUTTON);
        CHECK(application_runtime_accept_remote_stop(
            &runtime, &request, source, &action) == NOSTOS_OK);
        CHECK(action == APPLICATION_REMOTE_STOP_FORWARD_UART);
    }
    nostos_message_t overflow = remote_stop(
        2U, 7001U, NOSTOS_STOP_REASON_BUTTON);
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &overflow, 2U, &action) == NOSTOS_TOO_LARGE);

    nostos_message_t duplicate = remote_stop(
        2U, 6001U, NOSTOS_STOP_REASON_BUTTON);
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &duplicate, 2U, &action) == NOSTOS_OK);
    CHECK(action == APPLICATION_REMOTE_STOP_FORWARD_UART);

    nostos_message_t ack = local_stop_ack(6001U);
    uint8_t remote_source = 0U;
    CHECK(application_runtime_complete_remote_stop(
        &runtime, &ack, &remote_source) == NOSTOS_OK);
    CHECK(application_runtime_accept_remote_stop(
        &runtime, &overflow, 2U, &action) == NOSTOS_OK);
}

int main(void)
{
    test_state_binding_freshness_and_repeat();
    test_local_source_stamping();
    test_state_latest_value_replaces_older_value();
    test_ten_node_capacity();
    test_stop_group_ack_partial_retry_and_dedupe();
    test_local_stop_mapping_replay_and_expiry();
    test_local_stop_fall_priority_and_new_identity();
    test_local_stop_rejects_zero_mesh_id_and_wraps_time();
    test_remote_stop_waits_for_local_ack_and_recovers_retries();
    test_remote_stop_rejects_ambiguous_source0_ack_ids();
    test_remote_stop_pending_capacity_is_bounded();
    puts("PASS application runtime state/stop policy");
    return 0;
}
