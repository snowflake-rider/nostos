#ifndef NOSTOS_MESH_RETRY_H
#define NOSTOS_MESH_RETRY_H

#include "nostos_bridge.h"

typedef struct {
    nostos_job_t job;
    bool pending;
} mesh_retry_slot_t;

void mesh_retry_init(mesh_retry_slot_t *slot);
bool mesh_retry_job_is_fall(const nostos_job_t *job);

/* One bounded retry slot with the same safety order as the bridge:
 * FALL/CLEAR > STOP > normal. Lower traffic never displaces higher traffic. */
nostos_result_t mesh_retry_store(
    mesh_retry_slot_t *slot,
    const nostos_job_t *job);

/* Returns the exact retained wire. Normal traffic expires at the bridge TTL;
 * FALL/CLEAR remains pending until a send succeeds. */
nostos_result_t mesh_retry_peek(
    mesh_retry_slot_t *slot,
    uint32_t now_ms,
    nostos_job_t *job);

void mesh_retry_complete(mesh_retry_slot_t *slot);

#endif
