#include "check.h"
#include "mesh_retry.h"

#include <stdio.h>
#include <string.h>

static nostos_job_t job(uint8_t type, uint16_t sequence, uint32_t received_ms)
{
    nostos_message_t message = {
        .type = type,
        .source_id = 1U,
        .session_id = 7U,
        .sequence = sequence,
    };
    if (type == NOSTOS_FALL || type == NOSTOS_FALL_CLEAR) {
        message.payload.incident = (nostos_incident_ref_t){7U, 1U};
    }
    nostos_job_t value = {
        .received_ms = received_ms,
        .direction = NOSTOS_TO_MESH,
    };
    CHECK(nostos_message_encode(
        &message, value.wire, sizeof(value.wire), &value.length) == NOSTOS_OK);
    return value;
}

int main(void)
{
    mesh_retry_slot_t slot;
    mesh_retry_init(&slot);
    nostos_job_t output = {0};
    CHECK(mesh_retry_peek(&slot, 0U, &output) == NOSTOS_EMPTY);

    nostos_job_t normal = job(NOSTOS_SPEED_UP, 2U, 100U);
    CHECK(mesh_retry_store(&slot, &normal) == NOSTOS_OK);
    CHECK(mesh_retry_peek(
        &slot, 100U + NOSTOS_BRIDGE_MAX_AGE_MS, &output) == NOSTOS_OK);
    CHECK(output.length == normal.length);
    CHECK(memcmp(output.wire, normal.wire, normal.length) == 0);
    CHECK(mesh_retry_peek(
        &slot, 101U + NOSTOS_BRIDGE_MAX_AGE_MS, &output) == NOSTOS_EXPIRED);
    CHECK(!slot.pending);

    normal = job(NOSTOS_SPEED_UP, 3U, 200U);
    nostos_job_t stop = job(NOSTOS_STOP, 4U, 201U);
    nostos_job_t fall = job(NOSTOS_FALL, 5U, 202U);
    CHECK(mesh_retry_store(&slot, &normal) == NOSTOS_OK);
    CHECK(mesh_retry_store(&slot, &stop) == NOSTOS_OK);
    CHECK(slot.job.wire[1] == NOSTOS_STOP);
    CHECK(mesh_retry_store(&slot, &fall) == NOSTOS_OK);
    CHECK(mesh_retry_peek(&slot, UINT32_MAX, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_FALL);
    CHECK(mesh_retry_store(&slot, &normal) == NOSTOS_FULL);
    mesh_retry_complete(&slot);
    CHECK(!slot.pending);

    puts("PASS one-slot Mesh retry retains exact wire and normal TTL");
    puts("PASS retry replacement preserves FALL/CLEAR > STOP > normal");
    return 0;
}
