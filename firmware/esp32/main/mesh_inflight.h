#ifndef NOSTOS_MESH_INFLIGHT_H
#define NOSTOS_MESH_INFLIGHT_H

#include "nostos_bridge.h"

typedef struct {
    nostos_job_t job;
    int completion_error;
    bool active;
    bool completion_pending;
} mesh_inflight_t;

void mesh_inflight_init(mesh_inflight_t *state);
nostos_result_t mesh_inflight_begin(
    mesh_inflight_t *state,
    const nostos_job_t *job);
nostos_result_t mesh_inflight_complete(
    mesh_inflight_t *state,
    int error);
nostos_result_t mesh_inflight_take_completion(
    mesh_inflight_t *state,
    nostos_job_t *job,
    int *error);
nostos_result_t mesh_inflight_cancel(
    mesh_inflight_t *state,
    nostos_job_t *job);
bool mesh_inflight_active(const mesh_inflight_t *state);

#endif
