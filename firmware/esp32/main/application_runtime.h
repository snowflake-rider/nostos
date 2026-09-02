#ifndef NOSTOS_APPLICATION_RUNTIME_H
#define NOSTOS_APPLICATION_RUNTIME_H

#include "nostos_protocol.h"

#define APPLICATION_RUNTIME_MAX_NODES 10U
#define APPLICATION_RUNTIME_STATE_REPEAT_MS 2000U
#define APPLICATION_RUNTIME_STATE_STALE_MS 6000U
#define APPLICATION_RUNTIME_STATE_UNKNOWN_MS 20000U
#define APPLICATION_RUNTIME_STOP_RETRY_MS 1000U
#define APPLICATION_RUNTIME_LOCAL_STOP_REPLAY_MS 2000U
#define APPLICATION_RUNTIME_REQUEST_DEDUPE_CAPACITY 16U
#define APPLICATION_RUNTIME_REMOTE_STOP_CAPACITY 10U

typedef enum {
    APPLICATION_STATE_UNKNOWN = 0,
    APPLICATION_STATE_FRESH,
    APPLICATION_STATE_STALE,
} application_state_freshness_t;

typedef struct {
    nostos_message_t message;
    uint32_t received_ms;
    bool occupied;
} application_state_entry_t;

typedef struct {
    uint8_t source_node_id;
    uint8_t message_type;
    uint32_t request_id;
} application_request_seen_t;

typedef enum {
    APPLICATION_LOCAL_STOP_NEW_MESH = 0,
    APPLICATION_LOCAL_STOP_ACK_ONLY,
} application_local_stop_action_t;

typedef enum {
    APPLICATION_REMOTE_STOP_FORWARD_UART = 0,
    APPLICATION_REMOTE_STOP_ACK_COMPLETED,
} application_remote_stop_action_t;

typedef struct {
    nostos_message_t request;
    bool occupied;
} application_remote_stop_handoff_t;

typedef struct {
    uint8_t local_source_node_id;
    uint8_t active_publisher_source[2];
    uint16_t peer_mask;
    application_state_entry_t states[2];
    nostos_message_t published_states[2];
    uint32_t next_state_publish_ms[2];
    bool state_publisher_active[2];
    application_request_seen_t request_seen[
        APPLICATION_RUNTIME_REQUEST_DEDUPE_CAPACITY];
    size_t request_seen_next;
    application_remote_stop_handoff_t remote_stop_handoffs[
        APPLICATION_RUNTIME_REMOTE_STOP_CAPACITY];
    uint32_t active_stop_request_id;
    uint32_t stop_retry_at_ms;
    uint16_t stop_pending_mask;
    nostos_stop_reason_t active_stop_reason;
    bool stop_active;
    uint32_t local_stop_request_id;
    uint32_t local_stop_mesh_request_id;
    uint32_t local_stop_replay_until_ms;
    nostos_stop_reason_t local_stop_reason;
    bool local_stop_replay_valid;
} application_runtime_t;

nostos_result_t application_runtime_init(
    application_runtime_t *runtime,
    uint8_t local_source_node_id,
    uint16_t peer_mask,
    uint8_t ride_publisher_source,
    uint8_t environment_publisher_source);

/* Accepts local source 0 or the already-stamped local id. Any other claimed
 * source is rejected before the normal Mesh codec can be reached. */
nostos_result_t application_runtime_stamp_local(
    const application_runtime_t *runtime,
    nostos_message_t *message);

/* Store one locally-produced state and make it immediately due for Mesh.
 * Repeated publications are generated every two seconds from the exact latest
 * state; no received Mesh state becomes a publisher. */
nostos_result_t application_runtime_publish_state(
    application_runtime_t *runtime,
    const nostos_message_t *message,
    uint32_t now_ms);

nostos_result_t application_runtime_take_due_state(
    application_runtime_t *runtime,
    uint32_t now_ms,
    nostos_message_t *message);

/* `bound_source_node_id` is derived from Mesh metadata, never from payload. */
nostos_result_t application_runtime_accept_mesh_state(
    application_runtime_t *runtime,
    const nostos_message_t *message,
    uint8_t bound_source_node_id,
    uint32_t now_ms);

application_state_freshness_t application_runtime_state(
    const application_runtime_t *runtime,
    nostos_topic_t topic,
    uint8_t publisher_source_node_id,
    uint32_t now_ms,
    nostos_message_t *message,
    bool *sensor_valid);

nostos_result_t application_runtime_begin_stop_message(
    application_runtime_t *runtime,
    const nostos_message_t *request,
    uint32_t now_ms);

/* Translate the paired STM32's boot-local STOP id into a Mesh-wide id.
 * NEW_MESH means the peer retry state was committed before this function
 * returned and the caller may now ACK the local id and send mesh_request.
 * ACK_ONLY means the local request was replayed inside the bounded RAM window;
 * the caller ACKs the local id but must not send another Mesh STOP.
 *
 * A FALL promotes a replayed BUTTON even when the local id is unchanged. A
 * BUTTON never downgrades a replayed FALL. The generated Mesh id must always
 * be nonzero, including on replay calls, so callers cannot accidentally rely
 * on whether a request will be classified as new. */
nostos_result_t application_runtime_accept_local_stop(
    application_runtime_t *runtime,
    const nostos_message_t *local_request,
    uint32_t generated_mesh_request_id,
    uint32_t now_ms,
    application_local_stop_action_t *action,
    nostos_message_t *mesh_request);

/* Check and commit are separate so STOP is remembered and ACKed only after
 * the frame has been handed to the paired STM32. */
nostos_result_t application_runtime_request_is_duplicate(
    const application_runtime_t *runtime,
    const nostos_message_t *request,
    uint8_t bound_source_node_id,
    bool *duplicate);

nostos_result_t application_runtime_remember_request(
    application_runtime_t *runtime,
    const nostos_message_t *request,
    uint8_t bound_source_node_id);

/* Stage a Mesh STOP until the paired STM32 returns a local source-0 STOP_ACK.
 * First delivery and pending duplicates both request UART forwarding. Once the
 * local ACK commits the request to completed dedupe, Mesh duplicates can be
 * ACKed immediately without waking STM32 again. Request ids are kept unique
 * across pending and retained completed sources because the local ACK carries
 * no remote source and would otherwise be ambiguous. */
nostos_result_t application_runtime_accept_remote_stop(
    application_runtime_t *runtime,
    const nostos_message_t *request,
    uint8_t bound_source_node_id,
    application_remote_stop_action_t *action);

nostos_result_t application_runtime_complete_remote_stop(
    application_runtime_t *runtime,
    const nostos_message_t *local_ack,
    uint8_t *remote_source_node_id);

nostos_result_t application_runtime_accept_stop_ack(
    application_runtime_t *runtime,
    const nostos_message_t *ack,
    uint8_t bound_source_node_id);

/* Returns the currently missing peers once per retry interval. Callers retry
 * only those peers by unicast. */
nostos_result_t application_runtime_stop_retry_due(
    application_runtime_t *runtime,
    uint32_t now_ms,
    uint16_t *missing_peer_mask,
    nostos_message_t *request);

uint16_t application_runtime_stop_pending_mask(
    const application_runtime_t *runtime);

#endif /* NOSTOS_APPLICATION_RUNTIME_H */
