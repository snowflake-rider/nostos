#ifndef NOSTOS_OUTPUT_COMMAND_RETRY_H
#define NOSTOS_OUTPUT_COMMAND_RETRY_H

#include "nostos_protocol.h"
#include "sensor_link.h"

#define OUTPUT_COMMAND_RETRY_DELAY_MS 100U

typedef struct {
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length;
    uint32_t command_id;
    uint32_t session_id;
    uint32_t next_attempt_ms;
    uint8_t message_type;
    uint8_t source_id;
    bool pending;
} output_command_retry_t;

void output_command_retry_init(output_command_retry_t *retry);

/* Retain the exact failed UART frame. Reusing its command_id makes retries
 * idempotent even when uart_wait_tx_done timed out after STM received it. */
nostos_result_t output_command_retry_store(
    output_command_retry_t *retry,
    const uint8_t *frame,
    size_t length,
    uint32_t command_id,
    uint32_t session_id,
    uint8_t message_type,
    uint8_t source_id,
    uint32_t now_ms);

/* READY is an explicit gate: a retained OUTPUT is never exposed to the UART
 * worker before the current ESP command epoch has been announced. */
nostos_result_t output_command_retry_peek(
    const output_command_retry_t *retry,
    bool ready,
    uint32_t now_ms,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *length,
    uint32_t *command_id,
    uint8_t *message_type,
    uint8_t *source_id);

nostos_result_t output_command_retry_finish(
    output_command_retry_t *retry,
    uint32_t command_id,
    bool succeeded,
    uint32_t now_ms);

bool output_command_retry_pending(const output_command_retry_t *retry);

/* A newly authenticated remote boot supersedes any unsent frame captured from
 * an older session of the same source. */
bool output_command_retry_discard_source_before_session(
    output_command_retry_t *retry,
    uint8_t source_id,
    uint32_t session_id);

#endif /* NOSTOS_OUTPUT_COMMAND_RETRY_H */
