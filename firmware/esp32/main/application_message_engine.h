#ifndef NOSTOS_APPLICATION_MESSAGE_ENGINE_H
#define NOSTOS_APPLICATION_MESSAGE_ENGINE_H

#include "nostos_state.h"
#include "sensor_link.h"
#include "shared_data_cache.h"

#define APPLICATION_MESSAGE_SNAPSHOT_CAPACITY (NOSTOS_NODE_COUNT * 3U)

typedef struct {
    uint32_t accepted;
    uint32_t duplicate;
    uint32_t rejected;
    uint32_t uart_tx_ok;
    uint32_t uart_tx_failed;
    uint32_t output_result_accepted;
    uint32_t output_result_duplicate;
    uint32_t output_result_rejected;
    uint32_t output_result_hardware_error;
    uint32_t output_result_unrecognized;
} application_message_engine_stats_t;

typedef struct {
    nostos_receiver_t receiver;
    shared_data_cache_t cache;
    application_message_engine_stats_t stats;
    uint32_t next_command_id;
    uint8_t local_source;
    bool initialized;
} application_message_engine_t;

/* One ESP-owned receiver is authoritative for local and Mesh traffic. */
nostos_result_t application_message_engine_init(
    application_message_engine_t *engine,
    uint8_t local_source,
    uint32_t local_session);

/* Call only after the Mesh source address has authenticated source_id. The
 * first observed sequence becomes the floor for a new remote boot session. */
nostos_result_t application_message_engine_approve_authenticated_session(
    application_message_engine_t *engine,
    uint8_t source_id,
    uint32_t session_id,
    uint16_t sequence_floor);

/* Local ESP-stamped and authenticated Mesh wires enter through this same
 * function. `accepted_message` is written only for a newly accepted packet. */
nostos_result_t application_message_engine_accept_wire(
    application_message_engine_t *engine,
    const uint8_t *wire,
    size_t length,
    uint32_t now_ms,
    nostos_message_t *accepted_message);

/* Convert one already accepted official message to a paired-STM output
 * command. command_id never uses zero and exhaustion fails closed. */
nostos_result_t application_message_engine_encode_output(
    application_message_engine_t *engine,
    const nostos_message_t *accepted_message,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length,
    uint32_t *command_id);

/* Freeze aggregate FALL semantics at acceptance time before priority queues
 * delay UART dispatch. A clear from one source remains FALL while any other
 * source incident is active. */
nostos_result_t application_message_engine_capture_output(
    const application_message_engine_t *engine,
    const nostos_message_t *accepted_message,
    nostos_message_t *captured_message);

/* Encode a message previously returned by capture_output without consulting
 * newer state that may have arrived while it waited in the output queue. */
nostos_result_t application_message_engine_encode_captured_output(
    application_message_engine_t *engine,
    const nostos_message_t *captured_message,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length,
    uint32_t *command_id);

/* HELLO recovery: active FALL incidents first, then fresh RIDE/ENVIRONMENT. */
nostos_result_t application_message_engine_snapshot(
    const application_message_engine_t *engine,
    uint32_t now_ms,
    nostos_message_t messages[APPLICATION_MESSAGE_SNAPSHOT_CAPACITY],
    size_t *message_count);

void application_message_engine_note_uart_tx(
    application_message_engine_t *engine,
    bool succeeded);

nostos_result_t application_message_engine_note_output_result(
    application_message_engine_t *engine,
    const sensor_link_output_result_t *result);

#endif /* NOSTOS_APPLICATION_MESSAGE_ENGINE_H */
