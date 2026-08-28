> 이관 원문: `communication-module/service/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Communication Service API

팀원용 진입점은 [comm.h](../../../code/communication-module/service/comm.h)다. 기존 event queue와 periodic을 하나의 서비스로 묶는다. 호출자는 큐를 직접 pop하거나 속도 전송 시각을 계산하지 않는다.

현재는 **송신 측 호스트 C API**다. 실제 패킷 인코딩, UART/BLE 어댑터, Mesh 송수신, B/C 수신 API는 포함하지 않는다. `comm_read_speed()`는 Head의 로컬 평균 조회이며 원격 Head 값 조회가 아니다.

## 누가 무엇을 호출하나

| API | 호출 시점 | 정상 반환 |
| --- | --- | --- |
| `comm_init()` | 시작 시 설정과 전송 콜백을 한 번 연결 | `COMM_OK` |
| `comm_post_button()` | 버튼 메시지 1, 2, 3이 새로 발생했을 때 | `COMM_OK`: 로컬 큐 접수 |
| `comm_update_speed()` | Head 센서 어댑터에서 새 속도 측정값을 얻었을 때 | `COMM_OK`: 샘플 저장 |
| `comm_process()` | main loop 또는 한 Task에서 반복 | `COMM_IDLE`, `COMM_BUSY`, `COMM_EVENT_ACCEPTED`, `COMM_SPEED_ACCEPTED` |
| `comm_read_speed()` | Head의 현재 평균과 유효성을 확인할 때 | `COMM_OK`, `COMM_NOT_READY`, `COMM_STALE` |
| `comm_invalidate_speed()` | 센서 연결 해제나 센서 오류가 확정됐을 때 | `COMM_OK`: 로컬 윈도 초기화 |

입력 API는 전송 콜백을 호출하지 않는다. 오직 `comm_process()`가 전송 접수를 요청한다. 입력 성공과 전송 접수 성공은 다르며, 어느 쪽도 원격 노드의 수신 ACK가 아니다.

## 초기화와 사용

완전히 실행 가능한 예제는 [examples/main.c](../../../code/communication-module/service/examples/main.c)에 있다. 아래는 역할을 보여주는 발췌다. `app_send`는 어댑터가 구현할 콜백, `transport`는 그 콜백이 사용할 객체다.

```c
#include "comm.h"

comm_t comm;
const comm_config_t config = {
    .speed_period_ms = 200,
    .speed_stale_after_ms = 1000,
    .max_event_burst = 2,
    .send = app_send,
    .send_context = &transport
};
comm_status_t status = comm_init(&comm, &config, now_ms);
/* status == COMM_OK를 확인한 다음 아래 API를 사용한다. */

/* 버튼 이벤트가 새로 발생했을 때. FULL이면 이 이벤트는 접수되지 않았다. */
status = comm_post_button(&comm, COMM_BUTTON_MSG_1);

/* Head에서 새 측정마다 한 번. speed_cm_s는 해석/단위 변환된 실제 측정값이다. */
status = comm_update_speed(&comm, speed_cm_s, now_ms);

/* 한 실행 흐름의 반복 처리. BUSY이면 다음 처리 기회까지 기다린다. */
status = comm_process(&comm, now_ms);
```

모든 반환값은 호출자 정책에 따라 처리해야 한다. 예제의 200ms/1,000ms/2건은 강제 기본값이 아니다. 세 설정값은 모두 0보다 커야 한다. 특히 센서가 1초마다 측정한다면 만료를 1초로 설정하지 말고 측정 지연까지 고려해야 한다.

구조체는 스택이나 정적 저장소에 할당할 수 있고 malloc은 사용하지 않는다. `config`의 값은 복사하므로 초기화 뒤 config를 유지할 필요는 없다. 다만 `send_context`가 가리키는 객체는 사용하는 동안 살아 있어야 한다. NULL context를 사용하는 콜백도 가능하다.

## 처리 순서와 한도

예를 들어 `max_event_burst=2`이고 속도가 계속 due이며 버튼 큐가 계속 채워지면 접수 순서는 다음과 같다.

```text
버튼 → 버튼 → 속도 → 버튼 → 버튼 → 속도 → ...
```

- 한 `comm_process()`는 전송 콜백을 최대 한 번 호출한다. 내부 sleep이나 재시도 루프는 없다.
- 초기화/속도 접수 성공 이후 버튼 접수 건수를 센다. 버튼을 한도만큼 접수하면, 다음 처리에서 유효하고 due인 속도를 먼저 시도한다.
- 속도가 아직 due가 아니거나 준비되지 않았거나 만료됐으면 버튼을 계속 처리한다. 버튼 카운터는 한도에서 멈추며, due가 된 뒤에는 속도가 우선한다.
- 버튼 큐가 비어 있으면 한도를 채우지 않아도 due 속도를 바로 시도한다.
- 버튼 전송 성공일 때만 FIFO에서 제거한다. 속도 전송 성공일 때만 주기 슬롯을 소비하고 버튼 카운터를 0으로 돌린다.
- BUSY이면 선택한 메시지를 접수하지 않은 것으로 취급한다. 큐·주기 슬롯·버튼 카운터를 유지하며, 같은 호출에서 다른 종류로 우회하지 않는다.
- 속도 BUSY 후에는 다음 호출에서 최신 평균을 다시 읽는다. 만료되면 그 속도를 보내지 않고 버튼 처리가 가능해진다.

이는 **접수 건수 기준 공정성**이다. 처리 함수가 계속 호출되고 전송부가 접수 가능한 상태이며 속도가 유효하다는 조건에서 동작한다. 시간 단위의 지연 상한, 초당 전송량, 무선 수신 주기를 보장하는 실시간 스케줄러는 아니다. 실제 호출 주기와 전송부 대역폭은 보드 통합 단계에서 결정한다. BUSY 동안 무한 루프로 재호출하지 않는다.

주기 시각은 초기화 시각 기준을 유지하고 놓친 슬롯은 건너뛴다. 예를 들어 200ms 주기에서 950ms에 처음 속도가 접수되면 다음 기준은 1,000ms다. 누락된 네 건을 몰아서 보내지 않는다. 자세한 평균·주기 정책은 [periodic](../periodic/README.md)을 따른다.

## 반환값과 오류

- `COMM_FULL`: 8칸 이벤트 큐가 가득 찼으며 새 이벤트는 미접수다. 기존 이벤트는 유지한다. 호출자는 손실 보고/별도 보관 등 정책을 정해야 한다.
- `COMM_NOT_READY`: 평균을 낼 새 측정 5개가 아직 없다. `COMM_STALE`: 마지막 정상 측정이 만료됐다. read는 두 경우 `{average_cm_s=0, valid=false}`를 돌려준다. 이 숫자 0을 정지로 해석하지 않는다.
- `COMM_OK`인 read의 `valid=true`, 평균 0은 정상 측정된 정지다.
- `COMM_IDLE`: 지금 전송할 것이 없다. 센서 결측과 주기 미도래를 구분하려면 `comm_read_speed()`를 쓴다.
- `COMM_INVALID_ARGUMENT`: NULL 포인터, NULL 전송 함수, 0인 설정값. NULL context는 오류가 아니다.
- `COMM_INVALID_MESSAGE`: 1, 2, 3 이외의 버튼 코드. 정수 축소 전에 검사한다.
- `COMM_INVALID_SAMPLE`: 음수, NaN, Inf인 속도. 기존 정상 샘플과 시각을 갱신하지 않는다.
- `COMM_INVALID_TIME`: 직전 시간 관측보다 작은 `now_ms`. 같은 시각은 가능하다. 인자/시간 오류는 read 출력이나 대기 이벤트를 소비하지 않는다.

초기화는 반드시 성공한 뒤 사용한다. 성공한 재초기화는 큐, 평균, 우선권, 시간 기준을 모두 초기화하므로 실행 중 임의로 호출하지 않는다. 잘못된 설정으로 실패한 init은 기존 상태를 유지한다. 다른 함수의 시간/인자 오류도 처리 전에 거절된다.

## 시간·동시성·전송부 계약

모든 시간 인자는 같은 단조 증가 64-bit 밀리초 시계를 사용한다. update/read/invalidate/process가 시계를 공유하며, process는 IDLE/BUSY인 경우에도 시각을 관측한다. 입력 오류로 거절된 샘플이나 인자는 시계를 갱신하지 않는다. 버튼 post는 시각을 사용하지 않는다. 이는 이벤트 발생 시각을 wire packet에 담는 기능이 아니다.

32-bit HAL tick의 wraparound는 보드 어댑터에서 처리해야 한다. 원격 노드의 시각이나 벽시계를 섞지 않는다.

API는 단일 실행 흐름 전용이다. ISR, BLE callback, 다른 Task에서 동시에 직접 호출하면 안 된다. 실제 보드에서는 이들이 보낸 입력을 한 소유 Task로 전달한 뒤 여기의 API를 호출해야 한다. RTOS 동기화 코드는 아직 없다. `comm_t` 내부 멤버나 그 안의 queue/periodic API도 외부에서 직접 조작하지 않는다.

전송 콜백은 두 메시지 종류를 처리할 수 있어야 한다. true는 메시지를 호출 중 복사/처리해 접수했음을 뜻한다. 비동기 UART/BLE 송신이라면 자체 소유 버퍼로 복사한 후 반환한다. 임시 메시지 포인터를 보관하지 않는다. false는 부작용 없이 미접수임을 뜻한다. 영구 오류/비동기 실패/재전송/원격 ACK 상태는 아직 이 bool 인터페이스에 포함하지 않는다.

콜백은 빠르게 반환해야 하고 같은 `comm_t`의 어떤 API에도 재진입하지 않는다. 콜백이나 전송부가 계속 BUSY이면 전달 보장은 없다. 현재에는 timeout/backoff 자동 재시도 정책도 없다.

## 빌드와 검증

호스트 CMake target `comm_service`를 링크하면 공개 헤더와 기존 라이브러리 의존성이 함께 연결된다. 직접 `comm_event_queue`와 `comm_periodic`을 조합할 필요가 없다. 아직 ESP-IDF component나 STM32 패키지로 배포하는 단계는 아니다.

```cmake
target_link_libraries(your_app PRIVATE comm_service)
```

[상위 빌드 명령](../README.md)을 실행한 뒤:

```sh
./communication-module/build/test_comm
./communication-module/build/comm_service_demo
```

[테스트](../../../code/communication-module/service/tests/test_comm.c)는 공개 API와 가짜 전송 콜백만 사용한다. FIFO/BUSY, burst 공정성, 지속적인 혼합 부하, 최신 평균 재시도, 큐 초과, 결측/정지/만료, 단조 시계, 잘못된 입력, 설정 복사/재초기화, 독립 인스턴스, 64-bit 시간 상한 등 12개 동작을 검사한다.

실제 UART, Bluetooth 센서 수신, Mesh 발행/수신, B/C 전달, 수신 timeout은 이 테스트가 증명하지 않는다. `valid=false` 무효화 메시지도 아직 무선으로 발행하지 않는다.

macOS AppleClang의 Debug, Release, ASan+UBSan 빌드에서 전체 CTest 7/7 통과를 확인했다. LeakSanitizer는 검사에 포함하지 않는다.
