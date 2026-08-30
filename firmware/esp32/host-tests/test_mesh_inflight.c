#include "check.h"
#include "mesh_inflight.h"

#include <stdio.h>
#include <string.h>

static nostos_job_t make_job(uint16_t sequence)
{
    nostos_message_t message = {
        .type = NOSTOS_SPEED_UP,
        .source_id = 1U,
        .session_id = 9U,
        .sequence = sequence,
    };
    nostos_job_t job = {
        .received_ms = 100U,
        .direction = NOSTOS_TO_MESH,
    };
    CHECK(nostos_message_encode(
        &message, job.wire, sizeof(job.wire), &job.length) == NOSTOS_OK);
    return job;
}

int main(void)
{
    mesh_inflight_t state;
    mesh_inflight_init(&state);
    nostos_job_t first = make_job(3U);
    nostos_job_t second = make_job(4U);

    CHECK(mesh_inflight_begin(&state, &first) == NOSTOS_OK);
    CHECK(mesh_inflight_active(&state));
    CHECK(mesh_inflight_begin(&state, &second) == NOSTOS_CONFLICT);

    /* A callback may arrive immediately after begin(), before the send API
     * returns. The exact job was already captured and remains identifiable. */
    CHECK(mesh_inflight_complete(&state, -7) == NOSTOS_OK);
    CHECK(mesh_inflight_complete(&state, 0) == NOSTOS_STALE);
    nostos_job_t output = {0};
    CHECK(mesh_inflight_cancel(&state, &output) == NOSTOS_CONFLICT);
    int error = 0;
    CHECK(mesh_inflight_take_completion(
        &state, &output, &error) == NOSTOS_OK);
    CHECK(error == -7);
    CHECK(output.length == first.length);
    CHECK(memcmp(output.wire, first.wire, first.length) == 0);
    CHECK(!mesh_inflight_active(&state));

    CHECK(mesh_inflight_begin(&state, &second) == NOSTOS_OK);
    CHECK(mesh_inflight_cancel(&state, &output) == NOSTOS_OK);
    CHECK(output.wire[7] == second.wire[7]);
    CHECK(!mesh_inflight_active(&state));

    puts("PASS one in-flight Mesh job is serialized and callback-identifiable");
    puts("PASS immediate admission failure cancels the exact retained wire");
    return 0;
}
