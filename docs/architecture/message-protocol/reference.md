> 이관 주의: 원본의 추가 STM32 시험·진단 명령은 미통합 출력 시험 패치를 전제로 할 수 있습니다. 현재 팀 펌웨어에 자동 반영된 기능으로 해석하지 않습니다. [검증 안내](../../verification/index.md)를 먼저 확인하세요.

# Message Protocol — STM32 USART ↔ ESP32 Bluetooth Mesh

처음 배우거나 한꺼번에 읽기 어렵다면 **[메시지 프로토콜, 하나씩 배우기](01-first-steps.md)**의 1단계부터 시작하세요.

이 디렉터리는 **현재 메시지를 어떤 바이트로 만들고, 어느 함수에서 전달하는지** 정리한다. 새 프로토콜이나 펌웨어를 구현하는 디렉터리는 아니다.

기준: 2026-08-28의 STM32 통합 소스와 ESP32 **Layer 8** 소스. 보드 검증 결과는 별도의 [검증 기록](../../verification/03-button-to-mesh-test.md)을 따른다.

## 1. 먼저 전체 흐름

**STM32가 이벤트의 의미를 결정하고, ESP32가 Mesh 전송 형식으로 포장한다.** 현재는 문장·JSON·센서 실수값 대신 미리 정한 메시지 ID를 보낸다.

정지 요청 `MSG_STOP_REQUEST = 0x13`의 예:

```text
STM32 A                  ESP32 A                       ESP32 B
버튼 입력                 UART1 RX                      Mesh 수신
    │                        │                              │
정지 요청 결정               │                              │
    └─ USART1 TX: [13] ─────►│                              │
                             ├─ payload 작성: [01 13]       │
                             └─ Bluetooth Mesh / C001 ─────►│
                                                            ├─ 형식 검사
                                                            └─ ID 추출: 13
```

위 숫자는 모두 **16진수 바이트**다. `01 13`은 Mesh에 넘기는 앱 payload이며, 무선 패킷 전체가 2바이트라는 뜻은 아니다.

수신 ESP32에는 추출한 `13`을 UART1 TX로 자기 STM32에 전달하는 코드도 있다. 다만 위 경로의 상대 ESP32 수신과 상대 STM32의 실제 수신은 별도로 검증해야 한다.

## 2. 메시지 ID 약속

원본 정의: [STM32 message_type.h](../../../libs/protocol/message_type.h). Layer 8은 [로컬 복사본](../../../libs/protocol/message_type.h)을 사용한다.

| 의미 | C 이름 | STM32 → ESP32 UART | ESP32 → ESP32 Mesh payload |
| --- | --- | --- | --- |
| 감속 요청 | `MSG_SPEED_DOWN_REQUEST` | `10` | `01 10` |
| 가속 요청 | `MSG_SPEED_UP_REQUEST` | `11` | `01 11` |
| 안전 알림 | `MSG_SAFETY_REMINDER` | `12` | `01 12` |
| 정지 요청 | `MSG_STOP_REQUEST` | `13` | `01 13` |
| 후방 안전 | `MSG_REAR_SAFE` | `20` | `01 20` |
| 후방 경고 | `MSG_REAR_WARNING` | `21` | `01 21` |
| 낙차 감지 | `MSG_FALL_DETECTED` | `30` | `01 30` |
| 긴급 요청 | `MSG_SOS` | `31` | `01 31` |

`00`은 `MSG_NONE`으로 UART에서 무시한다. `FF`는 `MSG_UNKNOWN`이며 유효한 전송 이벤트가 아니다. 그 밖의 미정의 ID도 Layer 8에서 거부한다.

현재 이 경로에는 속도·거리 수치, 문자열, 앱 메시지 순번, 센서 측정 시각을 담는 필드가 없다. 예를 들어 `21`은 “후방 경고”이지 “거리 21cm”가 아니다.

## 3. STM32에서 메시지를 만드는 과정

현재 D10/PB6 테스트 버튼은 정지 요청을 만든다. [button.c](../../../experiments/stm32-output-test/README.md)는 LOW를 눌림으로 읽고, 30ms 디바운싱 후 **눌림 전이**에 `MSG_STOP_REQUEST`를 반환한다. 버튼을 계속 누르고 있는 동안 매 반복마다 보내는 구조는 아니다.

```text
button_get_message()                  버튼 → message_type_t
  → app_process()                    MSG_NONE이 아닌지 확인
  → message_router_publish_local()   로컬 알림 처리 + UART 전송 요청
  → uart_service_send_message()      uint8_t로 변환하여 1바이트 송신
  → HAL_UART_Transmit()              USART1 TX
```

실제 [uart_service.c](../../../firmware/stm32/MyApp/service/uart_service.c)의 핵심:

```c
uint8_t transmit_byte = (uint8_t)message;
uart_status = HAL_UART_Transmit(
    message_uart,
    &transmit_byte,
    1U,
    UART_TRANSMIT_TIMEOUT_MS
);
```

[main.c](../../../experiments/stm32-output-test/README.md)의 `app_init(&hspi2, &huart1, &hi2c1)`가 메시지 통신에 USART1을 연결한다. `1U`는 **1바이트 전송**, timeout 상수는 `10ms`다. enum이나 구조체 전체를 `sizeof(...)`로 보내지 않는다.

| 항목 | 현재 설정 |
| --- | --- |
| 송신 핀 → 수신 핀 | STM32 D8/PA9 USART1 TX → ESP32 GPIO18 UART1 RX |
| 통신 설정 | 115200 baud, 8 data bits, no parity, 1 stop bit, flow control 없음 |
| 앱 데이터 단위 | ID 1바이트 |
| 별도 앱 프레임 | 시작 표시·길이 필드·체크섬·종료 표시 없음 |
| 디버그 경로 | USART2/ST-LINK USB는 USART1 메시지 경로와 별개 |

`0x13` 한 바이트와 문자열 `"13"`은 다르다. 문자열 `"13"`은 ASCII `31 33` 두 바이트다. 특히 `31`은 유효한 **SOS ID**이므로 문자열을 보내면 잘못된 이벤트가 발생할 수 있다. 줄바꿈이나 로그 문자열도 데이터 UART에 섞지 않는다.

## 4. ESP32에서 Mesh 메시지를 만드는 과정

[bridge_runtime.c](../../../firmware/esp32/main/bridge_runtime.c)의 `uart_rx_task()`가 UART를 한 바이트씩 읽는다.

```text
uart_read_bytes(..., &byte, 1, ...)
  → event_bridge_uart()       유효 ID·Mesh 준비 상태 검사 → 큐에 저장
  → worker_task()
  → event_bridge_next()       큐에서 꺼내기 → 만료·준비 상태 검사
  → event_job_send()
  → event_encode()            ID 앞에 version 붙이기
  → send_mesh()
  → mesh_node_send_event()
  → esp_ble_mesh_server_model_send_msg()
```

[event_protocol.c](../../../libs/protocol/event_protocol.c)의 포장 부분은 다음 두 줄이다:

```c
wire[0] = EVENT_WIRE_VERSION;  // 0x01: 프로토콜 버전
wire[1] = id;                 // 0x13: 정지 요청 등
```

| payload 위치 | 크기 | 의미 | 정지 요청 예 |
| --- | --- | --- | --- |
| `wire[0]` | 1바이트 | 형식 버전 | `01` |
| `wire[1]` | 1바이트 | 이벤트 ID | `13` |

[mesh_node.c](../../../firmware/esp32/main/mesh_node.c)는 이 2바이트와 Vendor opcode를 Mesh API에 넘긴다.

| Mesh 항목 | 현재 코드 값 |
| --- | --- |
| 모델 | Vendor Model, Company ID `0x02E5`, Model ID `0x0001` |
| Opcode | `ESP_BLE_MESH_MODEL_OP_3(0x20, 0x02E5)` — 3바이트 opcode |
| 목적지 | 그룹 주소 `0xC001` |
| 전송 TTL | `7` |
| NetKey/AppKey index | 실제 모델 설정에서 얻은 값, `0`으로 고정하지 않음 |

`0x02E5`는 코드에서 Espressif 예제 기반의 제한된 프로토타입용 ID로 표시되어 있다.

이 그룹 전송은 같은 Mesh 네트워크·AppKey·Vendor Model 구독 설정을 갖춘 노드를 대상으로 한다. 주변의 모든 Bluetooth 기기에 보내는 것은 아니다. **앱 payload에는 source 주소나 TTL을 직접 붙이지 않는다.** 송신 주소·목적지·보안 등 Mesh 전송 처리는 스택에 맡긴다.

## 5. 상대 ESP32는 어떻게 읽나?

```text
custom_callback()              Vendor Model/opcode 수신 확인
  → bridge_runtime_mesh_rx()   payload + Mesh source 주소 전달
  → event_bridge_mesh()
  → event_decode()             길이 2 / version 1 / 유효 ID 검사
  → EVENT_TO_UART 큐에 저장    자기 source 메시지는 제외
  → worker_task()
  → event_job_send()
  → send_uart()                ID 1바이트만 UART1 TX로 출력
```

`event_decode()`가 `01 13`을 읽으면 버전을 확인한 뒤 `13`을 꺼낸다. source 주소는 payload가 아니라 Mesh 수신 context에서 얻는다. 큐의 `event_job_t`에 있는 source·수신 시각·방향은 **ESP32 내부 처리 정보**이며, 구조체 전체를 무선으로 보내지 않는다.

원격 수신 이벤트를 앱이 새 Mesh 메시지로 다시 보내지는 않는다. 다른 노드까지 중계하는 Mesh Relay와 이 앱의 UART 전달은 별개다.

## 6. 전달 보장과 현재 검증 경계

- Layer 8의 양방향 공유 큐는 32건이다. 가득 차면 새 입력을 버린다.
- 앱 입력 후 1000ms 이상 지난 작업은 꺼낼 때 버린다. Mesh 미준비 상태의 UART 입력도 버린다.
- 작업은 전송 전에 큐에서 제거한다. 전송 실패 시 앱 자동 재시도나 상대 수신 ACK는 없다.
- `MESH_TX ... api=accepted`는 로컬 API 수락이다. 상대 수신은 상대의 `MESH_RX`로 확인한다.
- 1바이트 UART 형식에는 앱 체크섬이 없으므로, 잡음이 유효한 ID 값이 되면 형식 검사만으로 구분할 수 없다.

**기존 실물 기록:** [2026-08-28 버튼 시험](../../verification/03-button-to-mesh-test.md)에 STM32 → D6 UART → 76 Mesh 수신이 세 번 관찰되어 있다. 이번 문서 작성에서 보드를 다시 시험하지는 않았다. 상대 STM32 수신, 제어된 다중 홉 Relay, reset 직후 안정성 등은 [미검증 항목과 한계](../../verification/04-recovery-and-limits.md)를 확인한다.

## 7. 다음에 코드를 볼 순서

| 읽을 파일 | 확인할 내용 |
| --- | --- |
| [message_type.h](../../../libs/protocol/message_type.h) | 메시지 이름과 번호 |
| [button.c](../../../experiments/stm32-output-test/README.md) | 버튼에서 ID 선택 |
| [app.c](../../../firmware/stm32/MyApp/ap/app.c) · [message_router.c](../../../firmware/stm32/MyApp/service/message_router.c) | 로컬 이벤트를 UART 전송으로 연결 |
| [uart_service.c](../../../firmware/stm32/MyApp/service/uart_service.c) | STM32의 실제 1바이트 송신 |
| [event_protocol.c](../../../libs/protocol/event_protocol.c) | ID ↔ 2바이트 payload 변환 |
| [event_bridge.c](../../../libs/protocol/event_bridge.c) | 큐·전송 방향·유효성·만료 |
| [bridge_runtime.c](../../../firmware/esp32/main/bridge_runtime.c) | UART Task와 송신 worker |
| [mesh_node.c](../../../firmware/esp32/main/mesh_node.c) | Vendor Model 전송과 수신 callback |

**수정 시 주의:** STM32 공통 프로토콜과 Layer 8의 `common/`은 자동 동기화되지 않는다. 메시지 ID나 payload 형식을 변경할 때 두 경로와 수신 노드의 호환성을 함께 확인한다. 기존 `communication-module/common/comm_message.h`는 별도의 메모리 API이며 이 경로의 wire 형식 정의가 아니다.

[8종 실제 송수신 테스트](../../verification/message-broadcast.md) · [프로젝트 처음으로](../../archive/imported/README.md) · [Layer 8 설정·실행 안내](../../../firmware/esp32/docs/layer8-background.md)
