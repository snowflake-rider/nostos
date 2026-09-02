#include "application_runtime.h"

#include <string.h>

static bool source_valid(uint8_t source_node_id)
{
    return source_node_id >= 1U &&
        source_node_id <= APPLICATION_RUNTIME_MAX_NODES;
}

static uint16_t source_bit(uint8_t source_node_id)
{
    return source_valid(source_node_id)
        ? (uint16_t)(1U << (source_node_id - 1U)) : 0U;
}

static bool deadline_pending(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) < 0;
}

static int topic_index(nostos_topic_t topic)
{
    if (topic == NOSTOS_TOPIC_RIDE) return 0;
    if (topic == NOSTOS_TOPIC_ENVIRONMENT) return 1;
    return -1;
}

static nostos_result_t validate_state(
    const nostos_message_t *message,
    uint8_t expected_source)
{
    if (message == NULL || !source_valid(expected_source)) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (message->type != NOSTOS_MESSAGE_STATE_UPDATE ||
        message->source_node_id != expected_source) {
        return NOSTOS_BAD_VALUE;
    }
    if (message->payload.state_update.schema_rev != NOSTOS_STATE_SCHEMA_REV ||
        topic_index((nostos_topic_t)message->payload.state_update.topic_id) < 0) {
        return NOSTOS_BAD_VALUE;
    }
    return NOSTOS_OK;
}

static void store_state(
    application_runtime_t *runtime,
    const nostos_message_t *message,
    uint32_t now_ms)
{
    int topic = topic_index((nostos_topic_t)message->payload.state_update.topic_id);
    runtime->states[topic] = (application_state_entry_t){
        .message = *message,
        .received_ms = now_ms,
        .occupied = true,
    };
}

nostos_result_t application_runtime_init(
    application_runtime_t *runtime,
    uint8_t local_source_node_id,
    uint16_t peer_mask,
    uint8_t ride_publisher_source,
    uint8_t environment_publisher_source)
{
    if (runtime == NULL || !source_valid(local_source_node_id) ||
        (peer_mask & (uint16_t)~0x03FFU) != 0U ||
        !source_valid(ride_publisher_source) ||
        !source_valid(environment_publisher_source)) {
        return NOSTOS_BAD_ARGUMENT;
    }
    *runtime = (application_runtime_t){
        .local_source_node_id = local_source_node_id,
        .active_publisher_source = {
            ride_publisher_source, environment_publisher_source,
        },
        .peer_mask = (uint16_t)(peer_mask &
            (uint16_t)~source_bit(local_source_node_id)),
    };
    return NOSTOS_OK;
}

nostos_result_t application_runtime_stamp_local(
    const application_runtime_t *runtime,
    nostos_message_t *message)
{
    if (runtime == NULL || message == NULL) return NOSTOS_BAD_ARGUMENT;
    if (message->source_node_id != NOSTOS_LOCAL_SOURCE_NODE_ID &&
        message->source_node_id != runtime->local_source_node_id) {
        return NOSTOS_BAD_VALUE;
    }
    message->source_node_id = runtime->local_source_node_id;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_publish_state(
    application_runtime_t *runtime,
    const nostos_message_t *message,
    uint32_t now_ms)
{
    if (runtime == NULL) return NOSTOS_BAD_ARGUMENT;
    nostos_result_t result = validate_state(
        message, runtime->local_source_node_id);
    if (result != NOSTOS_OK) return result;
    int topic = topic_index((nostos_topic_t)message->payload.state_update.topic_id);
    if (runtime->active_publisher_source[topic] !=
        runtime->local_source_node_id) {
        return NOSTOS_BAD_VALUE;
    }
    store_state(runtime, message, now_ms);
    runtime->published_states[topic] = *message;
    runtime->state_publisher_active[topic] = true;
    runtime->next_state_publish_ms[topic] = now_ms;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_take_due_state(
    application_runtime_t *runtime,
    uint32_t now_ms,
    nostos_message_t *message)
{
    if (runtime == NULL || message == NULL) return NOSTOS_BAD_ARGUMENT;
    for (size_t topic = 0U; topic < 2U; ++topic) {
        if (!runtime->state_publisher_active[topic] ||
            (int32_t)(now_ms - runtime->next_state_publish_ms[topic]) < 0) {
            continue;
        }
        *message = runtime->published_states[topic];
        runtime->next_state_publish_ms[topic] =
            now_ms + APPLICATION_RUNTIME_STATE_REPEAT_MS;
        return NOSTOS_OK;
    }
    return NOSTOS_EMPTY;
}

nostos_result_t application_runtime_accept_mesh_state(
    application_runtime_t *runtime,
    const nostos_message_t *message,
    uint8_t bound_source_node_id,
    uint32_t now_ms)
{
    if (runtime == NULL) return NOSTOS_BAD_ARGUMENT;
    nostos_result_t result = validate_state(message, bound_source_node_id);
    if (result != NOSTOS_OK) return result;
    int topic = topic_index((nostos_topic_t)message->payload.state_update.topic_id);
    if (runtime->active_publisher_source[topic] != bound_source_node_id) {
        return NOSTOS_BAD_VALUE;
    }
    store_state(runtime, message, now_ms);
    return NOSTOS_OK;
}

application_state_freshness_t application_runtime_state(
    const application_runtime_t *runtime,
    nostos_topic_t topic,
    uint8_t publisher_source_node_id,
    uint32_t now_ms,
    nostos_message_t *message,
    bool *sensor_valid)
{
    int index = topic_index(topic);
    if (runtime == NULL || index < 0 ||
        !source_valid(publisher_source_node_id)) {
        if (sensor_valid != NULL) *sensor_valid = false;
        return APPLICATION_STATE_UNKNOWN;
    }
    const application_state_entry_t *entry =
        &runtime->states[index];
    if (runtime->active_publisher_source[index] !=
        publisher_source_node_id) {
        if (sensor_valid != NULL) *sensor_valid = false;
        return APPLICATION_STATE_UNKNOWN;
    }
    if (!entry->occupied ||
        (uint32_t)(now_ms - entry->received_ms) >
            APPLICATION_RUNTIME_STATE_UNKNOWN_MS) {
        if (sensor_valid != NULL) *sensor_valid = false;
        return APPLICATION_STATE_UNKNOWN;
    }
    if (message != NULL) *message = entry->message;
    if (sensor_valid != NULL) {
        *sensor_valid = entry->message.payload.state_update.sensor_valid;
    }
    return (uint32_t)(now_ms - entry->received_ms) >
            APPLICATION_RUNTIME_STATE_STALE_MS
        ? APPLICATION_STATE_STALE : APPLICATION_STATE_FRESH;
}

nostos_result_t application_runtime_begin_stop_message(
    application_runtime_t *runtime,
    const nostos_message_t *request,
    uint32_t now_ms)
{
    if (runtime == NULL || request == NULL ||
        request->type != NOSTOS_MESSAGE_STOP_REQUEST ||
        request->source_node_id != runtime->local_source_node_id ||
        !nostos_request_id_valid(request->payload.stop_request.request_id)) {
        return NOSTOS_BAD_ARGUMENT;
    }
    runtime->active_stop_request_id = request->payload.stop_request.request_id;
    runtime->active_stop_reason =
        (nostos_stop_reason_t)request->payload.stop_request.reason;
    runtime->stop_pending_mask = runtime->peer_mask;
    runtime->stop_retry_at_ms = now_ms + APPLICATION_RUNTIME_STOP_RETRY_MS;
    runtime->stop_active = runtime->stop_pending_mask != 0U;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_accept_local_stop(
    application_runtime_t *runtime,
    const nostos_message_t *local_request,
    uint32_t generated_mesh_request_id,
    uint32_t now_ms,
    application_local_stop_action_t *action,
    nostos_message_t *mesh_request)
{
    if (runtime == NULL || local_request == NULL || action == NULL ||
        mesh_request == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (!nostos_request_id_valid(generated_mesh_request_id)) {
        return NOSTOS_BAD_VALUE;
    }
    if (local_request->type != NOSTOS_MESSAGE_STOP_REQUEST ||
        (local_request->source_node_id != NOSTOS_LOCAL_SOURCE_NODE_ID &&
         local_request->source_node_id != runtime->local_source_node_id) ||
        !nostos_request_id_valid(
            local_request->payload.stop_request.request_id)) {
        return NOSTOS_BAD_VALUE;
    }

    const uint32_t local_request_id =
        local_request->payload.stop_request.request_id;
    const nostos_stop_reason_t reason = (nostos_stop_reason_t)
        local_request->payload.stop_request.reason;

    /* Constructing through the protocol API also validates the reason. */
    nostos_message_t candidate;
    nostos_result_t result = nostos_message_make_stop(
        &candidate, runtime->local_source_node_id,
        generated_mesh_request_id, (uint8_t)reason);
    if (result != NOSTOS_OK) return result;

    const bool replay_active = runtime->local_stop_replay_valid &&
        deadline_pending(now_ms, runtime->local_stop_replay_until_ms);
    if (replay_active &&
        runtime->local_stop_request_id == local_request_id &&
        (runtime->local_stop_reason == reason ||
         (runtime->local_stop_reason == NOSTOS_STOP_REASON_FALL &&
          reason == NOSTOS_STOP_REASON_BUTTON))) {
        result = nostos_message_make_stop(
            mesh_request, runtime->local_source_node_id,
            runtime->local_stop_mesh_request_id,
            (uint8_t)runtime->local_stop_reason);
        if (result != NOSTOS_OK) return result;
        *action = APPLICATION_LOCAL_STOP_ACK_ONLY;
        return NOSTOS_OK;
    }

    result = application_runtime_begin_stop_message(
        runtime, &candidate, now_ms);
    if (result != NOSTOS_OK) return result;
    runtime->local_stop_request_id = local_request_id;
    runtime->local_stop_mesh_request_id = generated_mesh_request_id;
    runtime->local_stop_reason = reason;
    runtime->local_stop_replay_until_ms =
        now_ms + APPLICATION_RUNTIME_LOCAL_STOP_REPLAY_MS;
    runtime->local_stop_replay_valid = true;
    *mesh_request = candidate;
    *action = APPLICATION_LOCAL_STOP_NEW_MESH;
    return NOSTOS_OK;
}

static nostos_result_t request_identity(
    const nostos_message_t *request,
    uint8_t bound_source_node_id,
    uint32_t *request_id)
{
    if (request == NULL || request_id == NULL ||
        !source_valid(bound_source_node_id)) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (request->source_node_id != bound_source_node_id) {
        return NOSTOS_BAD_VALUE;
    }
    if (request->type == NOSTOS_MESSAGE_STOP_REQUEST) {
        *request_id = request->payload.stop_request.request_id;
    } else if (request->type == NOSTOS_MESSAGE_PACE_REQUEST) {
        *request_id = request->payload.pace_request.request_id;
    } else {
        return NOSTOS_BAD_VALUE;
    }
    return nostos_request_id_valid(*request_id) ? NOSTOS_OK : NOSTOS_BAD_VALUE;
}

nostos_result_t application_runtime_request_is_duplicate(
    const application_runtime_t *runtime,
    const nostos_message_t *request,
    uint8_t bound_source_node_id,
    bool *duplicate)
{
    if (runtime == NULL || duplicate == NULL) return NOSTOS_BAD_ARGUMENT;
    uint32_t request_id = 0U;
    nostos_result_t result = request_identity(
        request, bound_source_node_id, &request_id);
    if (result != NOSTOS_OK) return result;
    for (size_t i = 0U;
         i < APPLICATION_RUNTIME_REQUEST_DEDUPE_CAPACITY; ++i) {
        if (runtime->request_seen[i].source_node_id == bound_source_node_id &&
            runtime->request_seen[i].message_type == request->type &&
            runtime->request_seen[i].request_id == request_id) {
            *duplicate = true;
            return NOSTOS_OK;
        }
    }
    *duplicate = false;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_remember_request(
    application_runtime_t *runtime,
    const nostos_message_t *request,
    uint8_t bound_source_node_id)
{
    if (runtime == NULL) return NOSTOS_BAD_ARGUMENT;
    uint32_t request_id = 0U;
    nostos_result_t result = request_identity(
        request, bound_source_node_id, &request_id);
    if (result != NOSTOS_OK) return result;
    bool duplicate = false;
    result = application_runtime_request_is_duplicate(
        runtime, request, bound_source_node_id, &duplicate);
    if (result != NOSTOS_OK || duplicate) return result;
    runtime->request_seen[runtime->request_seen_next] =
        (application_request_seen_t){
            .source_node_id = bound_source_node_id,
            .message_type = request->type,
            .request_id = request_id,
        };
    runtime->request_seen_next = (runtime->request_seen_next + 1U) %
        APPLICATION_RUNTIME_REQUEST_DEDUPE_CAPACITY;
    return NOSTOS_OK;
}

static bool completed_stop_id_owned_by_other_source(
    const application_runtime_t *runtime,
    uint32_t request_id,
    uint8_t source_node_id)
{
    for (size_t i = 0U;
         i < APPLICATION_RUNTIME_REQUEST_DEDUPE_CAPACITY; ++i) {
        const application_request_seen_t *seen = &runtime->request_seen[i];
        if (seen->message_type == NOSTOS_MESSAGE_STOP_REQUEST &&
            seen->request_id == request_id &&
            seen->source_node_id != source_node_id) {
            return true;
        }
    }
    return false;
}

nostos_result_t application_runtime_accept_remote_stop(
    application_runtime_t *runtime,
    const nostos_message_t *request,
    uint8_t bound_source_node_id,
    application_remote_stop_action_t *action)
{
    if (runtime == NULL || action == NULL) return NOSTOS_BAD_ARGUMENT;
    uint32_t request_id = 0U;
    nostos_result_t result = request_identity(
        request, bound_source_node_id, &request_id);
    if (result != NOSTOS_OK) return result;
    if (request->type != NOSTOS_MESSAGE_STOP_REQUEST) return NOSTOS_BAD_VALUE;

    bool completed = false;
    result = application_runtime_request_is_duplicate(
        runtime, request, bound_source_node_id, &completed);
    if (result != NOSTOS_OK) return result;
    if (completed) {
        *action = APPLICATION_REMOTE_STOP_ACK_COMPLETED;
        return NOSTOS_OK;
    }
    if (completed_stop_id_owned_by_other_source(
            runtime, request_id, bound_source_node_id)) {
        return NOSTOS_BAD_VALUE;
    }

    application_remote_stop_handoff_t *free_entry = NULL;
    for (size_t i = 0U; i < APPLICATION_RUNTIME_REMOTE_STOP_CAPACITY; ++i) {
        application_remote_stop_handoff_t *entry =
            &runtime->remote_stop_handoffs[i];
        if (!entry->occupied) {
            if (free_entry == NULL) free_entry = entry;
            continue;
        }
        if (entry->request.payload.stop_request.request_id != request_id) {
            continue;
        }
        if (entry->request.source_node_id != bound_source_node_id ||
            entry->request.payload.stop_request.reason !=
                request->payload.stop_request.reason) {
            return NOSTOS_BAD_VALUE;
        }
        *action = APPLICATION_REMOTE_STOP_FORWARD_UART;
        return NOSTOS_OK;
    }
    if (free_entry == NULL) return NOSTOS_TOO_LARGE;
    *free_entry = (application_remote_stop_handoff_t){
        .request = *request,
        .occupied = true,
    };
    *action = APPLICATION_REMOTE_STOP_FORWARD_UART;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_complete_remote_stop(
    application_runtime_t *runtime,
    const nostos_message_t *local_ack,
    uint8_t *remote_source_node_id)
{
    if (runtime == NULL || local_ack == NULL ||
        remote_source_node_id == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (local_ack->type != NOSTOS_MESSAGE_STOP_ACK ||
        local_ack->source_node_id != NOSTOS_LOCAL_SOURCE_NODE_ID ||
        !nostos_request_id_valid(local_ack->payload.stop_ack.request_id)) {
        return NOSTOS_BAD_VALUE;
    }

    application_remote_stop_handoff_t *matched = NULL;
    for (size_t i = 0U; i < APPLICATION_RUNTIME_REMOTE_STOP_CAPACITY; ++i) {
        application_remote_stop_handoff_t *entry =
            &runtime->remote_stop_handoffs[i];
        if (!entry->occupied ||
            entry->request.payload.stop_request.request_id !=
                local_ack->payload.stop_ack.request_id) {
            continue;
        }
        if (matched != NULL) return NOSTOS_BAD_VALUE;
        matched = entry;
    }
    if (matched == NULL) return NOSTOS_EMPTY;

    const uint8_t source = matched->request.source_node_id;
    nostos_result_t result = application_runtime_remember_request(
        runtime, &matched->request, source);
    if (result != NOSTOS_OK) return result;
    *matched = (application_remote_stop_handoff_t){0};
    *remote_source_node_id = source;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_accept_stop_ack(
    application_runtime_t *runtime,
    const nostos_message_t *ack,
    uint8_t bound_source_node_id)
{
    if (runtime == NULL || ack == NULL ||
        !source_valid(bound_source_node_id)) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (ack->type != NOSTOS_MESSAGE_STOP_ACK ||
        ack->source_node_id != bound_source_node_id) {
        return NOSTOS_BAD_VALUE;
    }
    if (!runtime->stop_active ||
        ack->payload.stop_ack.request_id != runtime->active_stop_request_id) {
        return NOSTOS_EMPTY;
    }
    uint16_t bit = source_bit(bound_source_node_id);
    if ((runtime->stop_pending_mask & bit) == 0U) return NOSTOS_EMPTY;
    runtime->stop_pending_mask &= (uint16_t)~bit;
    if (runtime->stop_pending_mask == 0U) runtime->stop_active = false;
    return NOSTOS_OK;
}

nostos_result_t application_runtime_stop_retry_due(
    application_runtime_t *runtime,
    uint32_t now_ms,
    uint16_t *missing_peer_mask,
    nostos_message_t *request)
{
    if (runtime == NULL || missing_peer_mask == NULL || request == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (!runtime->stop_active || runtime->stop_pending_mask == 0U) {
        return NOSTOS_EMPTY;
    }
    if ((int32_t)(now_ms - runtime->stop_retry_at_ms) < 0) {
        return NOSTOS_EMPTY;
    }
    nostos_result_t result = nostos_message_make_stop(
        request, runtime->local_source_node_id,
        runtime->active_stop_request_id,
        (uint8_t)runtime->active_stop_reason);
    if (result != NOSTOS_OK) return result;
    *missing_peer_mask = runtime->stop_pending_mask;
    runtime->stop_retry_at_ms = now_ms + APPLICATION_RUNTIME_STOP_RETRY_MS;
    return NOSTOS_OK;
}

uint16_t application_runtime_stop_pending_mask(
    const application_runtime_t *runtime)
{
    return runtime != NULL ? runtime->stop_pending_mask : 0U;
}
