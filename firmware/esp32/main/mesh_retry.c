#include "mesh_retry.h"

void mesh_retry_init(mesh_retry_slot_t *slot)
{
    if (slot != NULL) *slot = (mesh_retry_slot_t){0};
}

bool mesh_retry_job_is_fall(const nostos_job_t *job)
{
    return job != NULL && job->length >= NOSTOS_HEADER_SIZE &&
        (job->wire[1] == NOSTOS_FALL ||
         job->wire[1] == NOSTOS_FALL_CLEAR);
}

static uint8_t job_priority(const nostos_job_t *job)
{
    if (mesh_retry_job_is_fall(job)) return 2U;
    if (job != NULL && job->length >= NOSTOS_HEADER_SIZE &&
        job->wire[1] == NOSTOS_STOP) {
        return 1U;
    }
    return 0U;
}

nostos_result_t mesh_retry_store(
    mesh_retry_slot_t *slot,
    const nostos_job_t *job)
{
    if (slot == NULL || job == NULL || job->direction != NOSTOS_TO_MESH ||
        job->length < NOSTOS_HEADER_SIZE || job->length > NOSTOS_WIRE_MAX) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (slot->pending &&
        job_priority(job) <= job_priority(&slot->job)) {
        return NOSTOS_FULL;
    }
    slot->job = *job;
    slot->pending = true;
    return NOSTOS_OK;
}

nostos_result_t mesh_retry_peek(
    mesh_retry_slot_t *slot,
    uint32_t now_ms,
    nostos_job_t *job)
{
    if (slot == NULL || job == NULL) return NOSTOS_BAD_ARGUMENT;
    if (!slot->pending) return NOSTOS_EMPTY;
    if (!mesh_retry_job_is_fall(&slot->job) &&
        (uint32_t)(now_ms - slot->job.received_ms) >
            NOSTOS_BRIDGE_MAX_AGE_MS) {
        mesh_retry_complete(slot);
        return NOSTOS_EXPIRED;
    }
    *job = slot->job;
    return NOSTOS_OK;
}

void mesh_retry_complete(mesh_retry_slot_t *slot)
{
    if (slot != NULL) *slot = (mesh_retry_slot_t){0};
}
