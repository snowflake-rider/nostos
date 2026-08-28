> 이관 원문: `communication-module/event-driven/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Event-driven Communication

버튼 담당 팀원이 판별한 메시지 1, 2, 3을 받아 발생 시점에 전송을 요청하는 경로다.

상태: C11 기반 고정 크기 FIFO 큐, 12개 동작 테스트, 가짜 전송 예제를 구현했다. 보드 Flash, UART, Bluetooth, Relay는 구현하거나 시험하지 않았다.

## 입력과 책임

- 입력: 판별 완료된 버튼 메시지 코드 1, 2, 3.
- 버튼 읽기, 디바운싱, 짧게/길게 누름 판별은 센서 담당 코드의 책임이다.
- 통신 모듈은 메시지를 접수하고 대기열에 보관하여 전송 계층에 넘긴다.
- 각 코드의 실제 의미는 팀원과 합의한다. 통신 모듈에서 임의로 정하지 않는다.

## 호스트 검증

1. 메시지 1, 2, 3을 넣으면 접수 순서대로 꺼낼 수 있다.
2. 지원하지 않는 코드를 넣으면 거절하고 대기열은 바꾸지 않는다.
3. 빈 대기열을 읽으면 데이터 없음으로 반환한다.
4. 정해진 용량을 초과하면 실패를 알리고 기존 이벤트를 조용히 덮어쓰지 않는다.
5. 전송 대역이 바쁘다고 응답하면 아직 넘기지 못한 이벤트를 보관한다.

주기 데이터의 다음 전송 시각을 기다려야만 이벤트를 처리하는 구조는 피한다. 다만 이벤트 우선순위는 실제 무선 전송이나 상대 도착 시간을 보장하지 않는다.

가짜 입력과 가짜 전송 대역으로 시작한다. 같은 코드의 연속 입력도 별개의 이벤트일 수 있으므로, 재시도 중복 제거와 단순한 코드 중복 제거를 혼동하지 않는다. 상대 수신 확인, 이벤트 식별자와 재시도 정책은 실제 통신 연결 전에 별도로 설계한다.

표준 Mesh Relay 자체는 이 대기열의 기능이 아니다. 이후 ESP32 Mesh 스택과 연결하여 검증한다.

## 파일과 API

- [comm_event_queue.h](../../../code/communication-module/event-driven/comm_event_queue.h): 큐 자료형과 공개 API.
- [comm_event_queue.c](../../../code/communication-module/event-driven/comm_event_queue.c): 8칸 ring buffer 구현.
- [공통 메시지](../../../code/communication-module/common/comm_message.h): 이벤트 종류와 데이터 정의.
- [사용 예제](../../../code/communication-module/event-driven/examples/main.c): BUSY 때 보관하고 접수 후 제거하는 흐름.
- [테스트](../../../code/communication-module/event-driven/tests/test_event_queue.c): FIFO, 용량 초과, wraparound, 값 복사, 반복 이벤트, NULL 인자, 재초기화, 독립 큐, 1,000회 반복 검증.

| API | 동작 |
| --- | --- |
| `comm_event_queue_init()` | 큐 초기화. 재호출 시 대기 중인 이벤트 삭제 |
| `comm_event_queue_push()` | 이벤트를 값으로 복사하여 저장 |
| `comm_event_queue_peek()` | 맨 앞 이벤트를 복사하되 유지 |
| `comm_event_queue_pop()` | 맨 앞 이벤트를 복사하고 제거 |

반환값은 `COMM_QUEUE_OK`, `COMM_QUEUE_EMPTY`, `COMM_QUEUE_FULL`, `COMM_QUEUE_INVALID_ARGUMENT`, `COMM_QUEUE_INVALID_MESSAGE`다. 실패 시 큐와 출력 인자는 변경하지 않는다. push는 입력 포인터, 메시지 종류와 코드, 큐 용량 순서로 검사한다.

```c
comm_event_queue_t queue;
comm_queue_status_t result = comm_event_queue_init(&queue);
if (result == COMM_QUEUE_OK) {
    const comm_message_t message = {
        .type = COMM_MESSAGE_EVENT,
        .data.event = {.code = COMM_BUTTON_MSG_1}
    };
    result = comm_event_queue_push(&queue, &message);
    /* result를 확인한다. FULL이면 접수되지 않은 이벤트다. */
}
```

전송 처리에서는 `peek -> 전송 계층 접수 -> pop` 순서로 사용한다. 전송 계층이 BUSY라면 pop하지 않는다. 전송 계층은 성공을 반환하기 전에 데이터를 복사하거나 처리를 완료해야 하며, 지역 변수의 포인터를 보관하면 안 된다. 이때 접수 성공은 상대 애플리케이션 수신 성공이 아니다.

## 사용 제한

- 사용 전에 반드시 init한다. 공개된 큐 멤버를 직접 수정하지 않는다.
- 모든 API는 단일 실행 흐름에서 호출한다. ISR/여러 Task에서 동시에 접근하지 않는다.
- peek부터 pop까지 다른 소비자가 큐를 변경해서는 안 된다. 전송 함수에서도 이 큐를 재진입하여 변경하지 않는다.
- 동시 접근이 필요해지면 별도 동기화 또는 RTOS 큐를 설계한다. `volatile`만 추가해서 해결하지 않는다.
- 동적 할당, 무선 전송, 자동 재시도, 중복 제거, 이벤트 만료는 이 큐에 포함하지 않는다.

## 검증 결과

이벤트 큐 최초 구현 시 AppleClang을 사용하는 macOS 호스트에서 Debug, Release, ASan+UBSan 빌드 모두 당시 CTest 2/2 통과를 확인했다. Release에서도 테스트 검사를 생략하지 않는다. ASan 메모리 접근 검사와 UBSan 검사는 수행했으며, 이 플랫폼이 지원하지 않는 LeakSanitizer 검사는 포함하지 않았다.

팀원용 통합 진입점은 [Service API](../service/README.md)다. Service가 소유한 큐는 직접 pop하지 않으며, 현재 전체 테스트 구성은 [상위 README](../README.md)를 따른다.

[상위 개발 범위와 빌드 명령](../README.md)
