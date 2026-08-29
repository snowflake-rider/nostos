#include "event_bridge.h"
#include "check.h"

typedef struct { unsigned mesh_calls, uart_calls; bool success; uint8_t bytes[2]; } fake_io_t;
static bool mesh_send(void *ctx, const uint8_t *bytes, size_t length)
{
    fake_io_t *io = ctx;
    CHECK(length == 2);
    io->mesh_calls++;
    io->bytes[0] = bytes[0]; io->bytes[1] = bytes[1];
    return io->success;
}
static bool uart_send(void *ctx, const uint8_t *bytes, size_t length)
{
    fake_io_t *io = ctx;
    CHECK(length == 1);
    io->uart_calls++;
    io->bytes[0] = bytes[0];
    return io->success;
}

static void capacity_and_fifo(void)
{
    event_bridge_t b;
    event_job_t job;
    event_bridge_init(&b);
    for (unsigned round = 0; round < 20; ++round) {
        for (unsigned i = 0; i < 32; ++i) {
            uint8_t wire[] = {1, 0x31};
            if (i % 2 == 0) CHECK(event_bridge_uart(&b, 0x10, 100 + i, true) == EVENT_OK);
            else CHECK(event_bridge_mesh(&b, wire, 2, (uint16_t)(i + 2), 1, 100 + i) == EVENT_OK);
        }
        CHECK(event_bridge_uart(&b, 0x20, 132, true) == EVENT_FULL);
        const uint8_t wire[] = {1, 0x20};
        CHECK(event_bridge_mesh(&b, wire, 2, 2, 1, 132) == EVENT_FULL);
        for (unsigned i = 0; i < 32; ++i) {
            CHECK(event_bridge_next(&b, 140, true, &job) == EVENT_OK);
            CHECK(job.received_ms == 100 + i);
            CHECK(job.id == (i % 2 == 0 ? 0x10 : 0x31));
        }
        CHECK(event_bridge_next(&b, 140, true, &job) == EVENT_EMPTY);
    }
    event_stats_t s = event_bridge_stats(&b);
    CHECK(s.full[EVENT_TO_MESH] == 20 && s.full[EVENT_TO_UART] == 20);
    for (unsigned i = 0; i < 32; ++i) CHECK(event_bridge_uart(&b, 0x10, i, true) == EVENT_OK);
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(event_bridge_next(&b, 40, true, &job) == EVENT_OK);
        CHECK(job.received_ms == i);
    }
    for (unsigned i = 32; i < 35; ++i) CHECK(event_bridge_uart(&b, 0x11, i, true) == EVENT_OK);
    for (unsigned i = 3; i < 35; ++i) {
        CHECK(event_bridge_next(&b, 40, true, &job) == EVENT_OK);
        CHECK(job.received_ms == i && job.id == (i < 32 ? 0x10 : 0x11));
    }
    CHECK(event_bridge_next(&b, 40, true, &job) == EVENT_EMPTY);
    puts("PASS bridge: shared 32-slot FIFO, reject newest, repeated IDs, wraparound");
}

static void rejection_and_expiry(void)
{
    event_bridge_t b;
    event_job_t job;
    event_bridge_init(&b);
    CHECK(event_bridge_uart(&b, 0, 0, true) == EVENT_NOOP);
    CHECK(event_bridge_uart(&b, 0xFF, 0, true) == EVENT_INVALID);
    CHECK(event_bridge_uart(&b, 0x10, 0, false) == EVENT_NOT_READY);
    CHECK(event_bridge_next(&b, 1, true, &job) == EVENT_EMPTY); /* never replay */
    CHECK(event_bridge_uart(&b, 0x10, 10, true) == EVENT_OK);
    CHECK(event_bridge_next(&b, 11, false, &job) == EVENT_NOT_READY);
    CHECK(event_bridge_next(&b, 12, true, &job) == EVENT_EMPTY);
    CHECK(event_bridge_uart(&b, 0x10, 100, true) == EVENT_OK);
    CHECK(event_bridge_next(&b, 1099, true, &job) == EVENT_OK);
    CHECK(event_bridge_uart(&b, 0x10, 100, true) == EVENT_OK);
    CHECK(event_bridge_next(&b, 1100, true, &job) == EVENT_EXPIRED);
    const uint8_t wire[] = {1, 0x20};
    CHECK(event_bridge_mesh(&b, wire, 2, 2, 1, 100) == EVENT_OK);
    CHECK(event_bridge_next(&b, 1101, true, &job) == EVENT_EXPIRED);
    CHECK(event_bridge_uart(&b, 0x10, 100, true) == EVENT_OK);
    CHECK(event_bridge_next(&b, 99, true, &job) == EVENT_EXPIRED); /* invalid clock */
    CHECK(event_bridge_uart(&b, 0x10, UINT64_C(0x100000000), true) == EVENT_OK);
    CHECK(event_bridge_next(&b, UINT64_C(0x100000001), true, &job) == EVENT_OK);
    CHECK(event_bridge_mesh(&b, wire, 1, 2, 1, 100) == EVENT_INVALID);
    CHECK(event_bridge_mesh(&b, NULL, 2, 2, 1, 100) == EVENT_INVALID);
    CHECK(event_bridge_mesh(&b, wire, 2, 0, 1, 100) == EVENT_INVALID);
    CHECK(event_bridge_mesh(&b, wire, 2, 0x8000, 1, 100) == EVENT_INVALID);
    event_stats_t s = event_bridge_stats(&b);
    CHECK(s.uart_noop == 1 && s.uart_invalid == 1 && s.not_ready == 2);
    CHECK(s.mesh_invalid == 4);
    CHECK(s.expired[EVENT_TO_MESH] == 2 && s.expired[EVENT_TO_UART] == 1);
    puts("PASS bridge: no-op/invalid, not-ready discard, 999/1000ms, 64-bit clock");
}

int main(void)
{
    capacity_and_fifo();
    rejection_and_expiry();
    event_bridge_t bridge;
    event_job_t job;
    event_bridge_init(&bridge);
    CHECK(event_bridge_uart(&bridge, 0x10, 100, true) == EVENT_OK);
    CHECK(event_bridge_next(&bridge, 101, true, &job) == EVENT_OK);
    CHECK(job.direction == EVENT_TO_MESH && job.id == 0x10);
    CHECK(event_bridge_next(&bridge, 101, true, &job) == EVENT_EMPTY);
    uint8_t wire[] = {1, 0x31};
    CHECK(event_bridge_mesh(&bridge, wire, 2, 0x1234, 0x0001, 200) == EVENT_OK);
    wire[1] = 0x10; /* callback-owned bytes may change after the call */
    CHECK(event_bridge_next(&bridge, 201, false, &job) == EVENT_OK);
    CHECK(job.direction == EVENT_TO_UART && job.id == 0x31 && job.source == 0x1234);
    CHECK(event_bridge_next(&bridge, 201, true, &job) == EVENT_EMPTY);
    CHECK(event_bridge_mesh(&bridge, wire, 2, 1, 1, 200) == EVENT_SELF);
    CHECK(event_bridge_next(&bridge, 201, true, &job) == EVENT_EMPTY);
    fake_io_t io = {.success = true};
    event_transport_t transport = {.context = &io, .mesh = mesh_send, .uart = uart_send};
    CHECK(event_bridge_uart(&bridge, 0x20, 300, true) == EVENT_OK);
    CHECK(event_bridge_next(&bridge, 301, true, &job) == EVENT_OK);
    CHECK(event_job_send(&job, &transport));
    event_bridge_complete(&bridge, job.direction, true);
    CHECK(io.mesh_calls == 1 && io.uart_calls == 0);
    CHECK(io.bytes[0] == 1 && io.bytes[1] == 0x20);
    CHECK(event_bridge_mesh(&bridge, wire, 2, 2, 1, 300) == EVENT_OK);
    CHECK(event_bridge_next(&bridge, 301, true, &job) == EVENT_OK);
    io.success = false;
    CHECK(!event_job_send(&job, &transport));
    event_bridge_complete(&bridge, job.direction, false);
    CHECK(io.mesh_calls == 1 && io.uart_calls == 1 && io.bytes[0] == 0x10);
    CHECK(event_bridge_next(&bridge, 301, true, &job) == EVENT_EMPTY);
    event_stats_t stats = event_bridge_stats(&bridge);
    CHECK(stats.accepted[EVENT_TO_MESH] == 1 && stats.failed[EVENT_TO_UART] == 1);
    CHECK(stats.mesh_self == 1);
    CHECK(event_bridge_uart(&bridge, 0x13, 400, true) == EVENT_OK);
    CHECK(event_bridge_next(&bridge, 401, true, &job) == EVENT_OK);
    CHECK(!event_job_send(&job, &transport));
    event_bridge_complete(&bridge, job.direction, false);
    CHECK(event_bridge_next(&bridge, 401, true, &job) == EVENT_EMPTY);
    CHECK(io.mesh_calls == 2); /* failed external send is not automatically retried */
    CHECK(event_bridge_mesh(&bridge, wire, 2, 2, 1, 400) == EVENT_OK);
    CHECK(event_bridge_next(&bridge, 401, true, &job) == EVENT_OK);
    io.success = true;
    CHECK(event_job_send(&job, &transport));
    event_bridge_complete(&bridge, job.direction, true);
    stats = event_bridge_stats(&bridge);
    CHECK(stats.failed[EVENT_TO_MESH] == 1 && stats.accepted[EVENT_TO_UART] == 1);
    CHECK(!event_job_send(NULL, &transport));
    CHECK(!event_job_send(&job, NULL));
    transport.uart = NULL;
    CHECK(!event_job_send(&job, &transport));
    puts("PASS bridge: directions, copied payload/source, self suppression");
    puts("PASS bridge: exact wire bytes, both API failures, no retries or RX republish");
    return 0;
}
