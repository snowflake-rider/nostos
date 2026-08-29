#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "comm.h"

typedef struct {
    bool ready;
    size_t accepted;
} demo_transport_t;

/* 호스트 예제 전용. 실제 UART/BLE 어댑터는 이후 이 콜백 자리에 연결한다. */
static bool mock_send(const comm_message_t *message, void *context)
{
    demo_transport_t *transport = context;
    if (!transport->ready) {
        return false;
    }
    /* 이번 예제는 호출 중 데이터를 소비한다. message 포인터를 보관하지 않는다. */
    if (message->type == COMM_MESSAGE_EVENT) {
        printf("MOCK_ACCEPT EVENT code=%u\n", (unsigned)message->data.event.code);
    } else if (message->type == COMM_MESSAGE_SPEED) {
        printf("MOCK_ACCEPT SPEED mean=%.1f cm/s valid=%d\n",
               (double)message->data.speed.average_cm_s, (int)message->data.speed.valid);
    } else {
        return false;
    }
    ++transport->accepted;
    return true;
}

static void require_status(comm_status_t actual, comm_status_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "Unexpected status: %d, expected %d\n", (int)actual, (int)expected);
        exit(EXIT_FAILURE);
    }
}

static void process_at(comm_t *comm, uint64_t now_ms, comm_status_t expected)
{
    const comm_status_t status = comm_process(comm, now_ms);
    require_status(status, expected);
    const char *name = "IDLE";
    if (status == COMM_EVENT_ACCEPTED) {
        name = "EVENT_ACCEPTED";
    } else if (status == COMM_SPEED_ACCEPTED) {
        name = "SPEED_ACCEPTED";
    } else if (status == COMM_BUSY) {
        name = "BUSY";
    }
    printf("t=%" PRIu64 " process=%s\n", now_ms, name);
}

int main(void)
{
    comm_t comm;
    demo_transport_t transport = {0};
    /* 예제 설정이다. 제품의 실제 센서 주기/대역폭에 맞춰 결정해야 한다. */
    const comm_config_t config = {
        .speed_period_ms = 200,
        .speed_stale_after_ms = 1000,
        .max_event_burst = 2,
        .send = mock_send,
        .send_context = &transport
    };
    require_status(comm_init(&comm, &config, 0), COMM_OK);

    comm_speed_data_t speed;
    require_status(comm_read_speed(&comm, 0, &speed), COMM_NOT_READY);
    printf("NO_SENSOR valid=%d\n", (int)speed.valid);

    /* 센서 어댑터가 새 측정값을 얻었을 때 호출할 API. 여기서는 가짜 입력이다. */
    const float samples[] = {20.0f, 22.0f, 18.0f, 25.0f, 15.0f};
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        require_status(comm_update_speed(&comm, samples[i], (uint64_t)i * 10), COMM_OK);
    }

    /* 버튼 담당 코드는 큐 구조를 몰라도 코드만 넘긴다. FULL 반환은 호출자가 처리한다. */
    require_status(comm_post_button(&comm, COMM_BUTTON_MSG_1), COMM_OK);
    require_status(comm_post_button(&comm, COMM_BUTTON_MSG_2), COMM_OK);
    require_status(comm_post_button(&comm, COMM_BUTTON_MSG_3), COMM_OK);
    puts("API queued buttons=1,2,3");

    process_at(&comm, 200, COMM_BUSY);
    transport.ready = true;
    process_at(&comm, 200, COMM_EVENT_ACCEPTED);
    process_at(&comm, 201, COMM_EVENT_ACCEPTED);
    transport.ready = false;
    process_at(&comm, 202, COMM_BUSY); /* 이번에는 속도 차례. 버튼 3으로 우회하지 않는다. */
    require_status(comm_update_speed(&comm, 30.0f, 210), COMM_OK);
    transport.ready = true;
    process_at(&comm, 210, COMM_SPEED_ACCEPTED); /* 이전 20이 아니라 최신 평균 22 */
    process_at(&comm, 210, COMM_EVENT_ACCEPTED);
    process_at(&comm, 210, COMM_IDLE);

    require_status(comm_read_speed(&comm, 1210, &speed), COMM_STALE);
    printf("NO_UPDATE valid=%d status=STALE\n", (int)speed.valid);
    process_at(&comm, 1210, COMM_IDLE);
    require_status(comm_post_button(&comm, COMM_BUTTON_MSG_1), COMM_OK);
    process_at(&comm, 1210, COMM_EVENT_ACCEPTED);
    require_status(comm_invalidate_speed(&comm, 1210), COMM_OK);
    require_status(comm_read_speed(&comm, 1210, &speed), COMM_NOT_READY);
    printf("DISCONNECTED valid=%d\n", (int)speed.valid);

    if (transport.accepted != 5) {
        return EXIT_FAILURE;
    }
    printf("MOCK_ACCEPTED=%zu\n", transport.accepted);
    puts("HOST_COMM_DEMO=PASS");
    return EXIT_SUCCESS;
}
