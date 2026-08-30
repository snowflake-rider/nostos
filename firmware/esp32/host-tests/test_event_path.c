#include "event_bridge.h"
#include "check.h"

/* UART/Mesh 전송 경계만 대체한다. ESP-IDF나 무선 그룹 수신의 모의 검증은 아니다. */
typedef struct {
    unsigned mesh_sends, uart_sends;
    uint8_t payload[2], last_uart;
} endpoint_t;

static bool capture_mesh(void *context, const uint8_t *bytes, size_t length)
{
    endpoint_t *endpoint = context;
    CHECK(length == 2);
    endpoint->payload[0] = bytes[0];
    endpoint->payload[1] = bytes[1];
    endpoint->mesh_sends++;
    return true;
}

static bool capture_uart(void *context, const uint8_t *bytes, size_t length)
{
    endpoint_t *endpoint = context;
    CHECK(length == 1);
    endpoint->last_uart = bytes[0];
    endpoint->uart_sends++;
    return true;
}

static void check_path(uint8_t id, size_t sender)
{
    event_bridge_t nodes[3];
    endpoint_t endpoints[3] = {0};
    event_transport_t transports[3];
    const uint16_t addresses[] = {0x0003, 0x0004, 0x0005};
    for (size_t i = 0; i < 3; ++i) {
        event_bridge_init(&nodes[i]);
        transports[i] = (event_transport_t){
            .context = &endpoints[i], .mesh = capture_mesh, .uart = capture_uart,
        };
    }

    /* 같은 버튼을 두 번 누르면 서로 다른 입력 두 건으로 전달되어야 한다. */
    for (unsigned press = 1; press <= 2; ++press) {
        event_job_t job;
        const uint64_t now = 1000U * press;
        CHECK(event_bridge_uart(&nodes[sender], id, now, true) == EVENT_OK);
        CHECK(event_bridge_next(&nodes[sender], now + 1, true, &job) == EVENT_OK);
        CHECK(event_job_send(&job, &transports[sender]));
        event_bridge_complete(&nodes[sender], job.direction, true);
        CHECK(endpoints[sender].payload[0] == 0x01);
        CHECK(endpoints[sender].payload[1] == id);
        CHECK(endpoints[sender].mesh_sends == press);

        /* 무선 수신을 가정하여 실제 공개 RX API에 payload를 각각 주입한다. */
        for (size_t receiver = 0; receiver < 3; ++receiver) {
            event_result_t result = event_bridge_mesh(&nodes[receiver],
                endpoints[sender].payload, 2, addresses[sender], addresses[receiver], now + 2);
            if (receiver == sender) {
                CHECK(result == EVENT_SELF);
                CHECK(endpoints[receiver].uart_sends == 0);
            } else {
                CHECK(result == EVENT_OK);
                /* 자기 송신 설정이 없어도 받은 이벤트는 UART로 내보낼 수 있다. */
                CHECK(event_bridge_next(&nodes[receiver], now + 3, false, &job) == EVENT_OK);
                CHECK(job.source == addresses[sender]);
                CHECK(event_job_send(&job, &transports[receiver]));
                event_bridge_complete(&nodes[receiver], job.direction, true);
                CHECK(endpoints[receiver].uart_sends == press);
                CHECK(endpoints[receiver].last_uart == id);
                CHECK(endpoints[receiver].mesh_sends == 0); /* RX를 재발행하지 않음 */
            }
            CHECK(event_bridge_next(&nodes[receiver], now + 4, true, &job) == EVENT_EMPTY);
        }
    }
}

int main(void)
{
    const uint8_t ids[] = {0x10, 0x11, 0x13, 0x30};
    for (size_t sender = 0; sender < 3; ++sender) {
        for (size_t i = 0; i < sizeof(ids); ++i) check_path(ids[i], sender);
    }
    puts("PASS event path: 4 allowed IDs, 3 simulated origins, repeated UART inputs");
    puts("PASS event path: exact 2-byte Mesh payload -> 1-byte peer UART, no self echo or RX republish");
    puts("HARDWARE_UART_AND_MESH=NOT_TESTED");
    return 0;
}
