#include "mesh_inflight.h"

void mesh_inflight_init(mesh_inflight_t *state)
{
    if (state != NULL) *state = (mesh_inflight_t){0};
}

nostos_result_t mesh_inflight_begin(
    mesh_inflight_t *state,
    const nostos_job_t *job)
{
    if (state == NULL || job == NULL || job->direction != NOSTOS_TO_MESH ||
        job->length < NOSTOS_HEADER_SIZE || job->length > NOSTOS_WIRE_MAX) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (state->active) return NOSTOS_CONFLICT;
    *state = (mesh_inflight_t){
        .job = *job,
        .active = true,
    };
    return NOSTOS_OK;
}

nostos_result_t mesh_inflight_complete(
    mesh_inflight_t *state,
    int error)
{
    if (state == NULL) return NOSTOS_BAD_ARGUMENT;
    if (!state->active || state->completion_pending) return NOSTOS_STALE;
    state->completion_error = error;
    state->completion_pending = true;
    return NOSTOS_OK;
}

nostos_result_t mesh_inflight_take_completion(
    mesh_inflight_t *state,
    nostos_job_t *job,
    int *error)
{
    if (state == NULL || job == NULL || error == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (!state->active || !state->completion_pending) return NOSTOS_EMPTY;
    *job = state->job;
    *error = state->completion_error;
    mesh_inflight_init(state);
    return NOSTOS_OK;
}

nostos_result_t mesh_inflight_cancel(
    mesh_inflight_t *state,
    nostos_job_t *job)
{
    if (state == NULL || job == NULL) return NOSTOS_BAD_ARGUMENT;
    if (!state->active) return NOSTOS_EMPTY;
    if (state->completion_pending) return NOSTOS_CONFLICT;
    *job = state->job;
    mesh_inflight_init(state);
    return NOSTOS_OK;
}

bool mesh_inflight_active(const mesh_inflight_t *state)
{
    return state != NULL && state->active;
}
