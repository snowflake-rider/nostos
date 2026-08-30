#include "output_command_retry.h"

#include <string.h>

void output_command_retry_init(output_command_retry_t *retry)
{
    if (retry != NULL) *retry = (output_command_retry_t){0};
}

nostos_result_t output_command_retry_store(
    output_command_retry_t *retry,
    const uint8_t *frame,
    size_t length,
    uint32_t command_id,
    uint32_t session_id,
    uint8_t message_type,
    uint8_t source_id,
    uint32_t now_ms)
{
    if (retry == NULL || frame == NULL || length == 0U ||
        length > SENSOR_LINK_FRAME_SIZE || command_id == 0U ||
        session_id == 0U ||
        source_id < 1U || source_id > NOSTOS_NODE_COUNT) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (retry->pending) return NOSTOS_FULL;
    *retry = (output_command_retry_t){
        .length = length,
        .command_id = command_id,
        .session_id = session_id,
        .next_attempt_ms = now_ms + OUTPUT_COMMAND_RETRY_DELAY_MS,
        .message_type = message_type,
        .source_id = source_id,
        .pending = true,
    };
    memcpy(retry->frame, frame, length);
    return NOSTOS_OK;
}

nostos_result_t output_command_retry_peek(
    const output_command_retry_t *retry,
    bool ready,
    uint32_t now_ms,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *length,
    uint32_t *command_id,
    uint8_t *message_type,
    uint8_t *source_id)
{
    if (retry == NULL || frame == NULL || length == NULL ||
        command_id == NULL || message_type == NULL || source_id == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (!retry->pending) return NOSTOS_EMPTY;
    if (!ready || (int32_t)(now_ms - retry->next_attempt_ms) < 0) {
        return NOSTOS_NOT_READY;
    }
    memcpy(frame, retry->frame, retry->length);
    *length = retry->length;
    *command_id = retry->command_id;
    *message_type = retry->message_type;
    *source_id = retry->source_id;
    return NOSTOS_OK;
}

nostos_result_t output_command_retry_finish(
    output_command_retry_t *retry,
    uint32_t command_id,
    bool succeeded,
    uint32_t now_ms)
{
    if (retry == NULL || command_id == 0U) return NOSTOS_BAD_ARGUMENT;
    if (!retry->pending) return NOSTOS_EMPTY;
    if (retry->command_id != command_id) return NOSTOS_CONFLICT;
    if (succeeded) {
        *retry = (output_command_retry_t){0};
    } else {
        retry->next_attempt_ms = now_ms + OUTPUT_COMMAND_RETRY_DELAY_MS;
    }
    return NOSTOS_OK;
}

bool output_command_retry_pending(const output_command_retry_t *retry)
{
    return retry != NULL && retry->pending;
}

bool output_command_retry_discard_source_before_session(
    output_command_retry_t *retry,
    uint8_t source_id,
    uint32_t session_id)
{
    if (retry == NULL || source_id < 1U || source_id > NOSTOS_NODE_COUNT ||
        session_id == 0U || !retry->pending ||
        retry->source_id != source_id || retry->session_id >= session_id) {
        return false;
    }
    *retry = (output_command_retry_t){0};
    return true;
}
