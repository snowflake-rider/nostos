#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "comm_periodic.h"

typedef struct {
    bool ready;
    size_t accepted;
} demo_transport_t;

/* 실제 Bluetooth 송신이 아니다. 호출 중에만 메시지를 사용한다. */
static bool mock_send(const comm_message_t *message, void *context)
{
    demo_transport_t *transport = context;
    if (!transport->ready || message->type != COMM_MESSAGE_SPEED ||
        !message->data.speed.valid) {
        return false;
    }
    ++transport->accepted;
    return true;
}

static const char *status_name(comm_periodic_status_t status)
{
    switch (status) {
    case COMM_PERIODIC_NOT_READY: return "NOT_READY";
    case COMM_PERIODIC_NOT_DUE: return "NOT_DUE";
    case COMM_PERIODIC_BUSY: return "BUSY";
    case COMM_PERIODIC_SENT: return "SENT";
    case COMM_PERIODIC_STALE: return "STALE";
    default: return "UNEXPECTED";
    }
}

int main(void)
{
    comm_periodic_t state;
    comm_speed_data_t speed;
    demo_transport_t transport = {.ready = true};
    const float inputs[] = {20.0f, 22.0f, 18.0f, 25.0f, 15.0f,
                            20.0f, 21.0f, 19.0f, 20.5f, 20.0f};
    const comm_periodic_status_t expected[] = {
        COMM_PERIODIC_NOT_READY, COMM_PERIODIC_NOT_READY,
        COMM_PERIODIC_NOT_READY, COMM_PERIODIC_NOT_READY,
        COMM_PERIODIC_BUSY, COMM_PERIODIC_SENT, COMM_PERIODIC_SENT,
        COMM_PERIODIC_NOT_DUE, COMM_PERIODIC_SENT, COMM_PERIODIC_NOT_DUE
    };

    if (comm_periodic_init(&state, 200, 1000, 0) != COMM_PERIODIC_OK ||
        comm_periodic_read(&state, 0, &speed) != COMM_PERIODIC_NOT_READY || speed.valid) {
        return EXIT_FAILURE;
    }
    puts("NO_SENSOR valid=0");

    /* 가짜 측정은 100ms마다, 전송 기준은 200ms마다다. */
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        const uint64_t now_ms = (uint64_t)i * 100;
        if (comm_periodic_update(&state, inputs[i], now_ms) != COMM_PERIODIC_OK) {
            return EXIT_FAILURE;
        }
        transport.ready = i != 4;
        const comm_periodic_status_t status =
            comm_periodic_poll(&state, now_ms, mock_send, &transport);
        const comm_periodic_status_t read_status = comm_periodic_read(&state, now_ms, &speed);
        if (status != expected[i] ||
            (read_status != COMM_PERIODIC_OK && read_status != COMM_PERIODIC_NOT_READY)) {
            return EXIT_FAILURE;
        }
        if (speed.valid) {
            printf("t=%" PRIu64 " raw=%.1f mean=%.1f status=%s\n",
                   now_ms, (double)inputs[i], (double)speed.average_cm_s, status_name(status));
        } else {
            printf("t=%" PRIu64 " raw=%.1f mean=NA status=%s\n",
                   now_ms, (double)inputs[i], status_name(status));
        }
    }

    if (comm_periodic_poll(&state, 1000, mock_send, &transport) != COMM_PERIODIC_SENT ||
        comm_periodic_read(&state, 1000, &speed) != COMM_PERIODIC_OK) {
        return EXIT_FAILURE;
    }
    printf("FINAL_MEAN mean=%.1f status=SENT\n", (double)speed.average_cm_s);

    if (comm_periodic_poll(&state, 1900, mock_send, &transport) != COMM_PERIODIC_STALE ||
        comm_periodic_read(&state, 1900, &speed) != COMM_PERIODIC_STALE || speed.valid) {
        return EXIT_FAILURE;
    }
    puts("NO_UPDATE valid=0 status=STALE");

    if (comm_periodic_invalidate(&state, 2000) != COMM_PERIODIC_OK) {
        return EXIT_FAILURE;
    }
    for (uint64_t i = 1; i <= 5; ++i) {
        if (comm_periodic_update(&state, 0.0f, 2000 + i * 10) != COMM_PERIODIC_OK) {
            return EXIT_FAILURE;
        }
    }
    if (comm_periodic_poll(&state, 2050, mock_send, &transport) != COMM_PERIODIC_SENT ||
        comm_periodic_read(&state, 2050, &speed) != COMM_PERIODIC_OK ||
        !speed.valid || speed.average_cm_s != 0.0f || transport.accepted != 5) {
        return EXIT_FAILURE;
    }
    puts("STOPPED mean=0.0 valid=1 status=SENT");
    printf("MOCK_ACCEPTED=%zu\n", transport.accepted);
    puts("HOST_PERIODIC_DEMO=PASS");
    return EXIT_SUCCESS;
}
