# 통신 모듈 상세 참고

처음 읽는 문서는 [쉬운 설명](README.md)입니다. 여기에는 설정값·오류 처리·내부 API를 자세히 남겨 두었습니다. 처음부터 모두 읽지 않아도 됩니다.

[저장소 시작](../../README.md) · [공통 이벤트/API](../../common/protocol/README.md) · [상세 설계](../../docs/superpowers/specs/2026-08-28-stm32-mesh-event-bridge-design.md) · [검증 기록](VERIFICATION.md)

**현재: 소스 구현·호스트 검사·양쪽 MCU 빌드 완료. 새 펌웨어의 Flash/실물 UART/Mesh는 미검증.**

STM32는 센서 읽기·판정·출력을 담당하고, ESP32는 이미 정해진 이벤트를 공유한다. 모든 라이더에 같은 송수신 펌웨어를 사용한다.

```text
센서/버튼 → STM32 A → UART1 → ESP32 A
                              │ Vendor Model / group C001
LED/음성 ← STM32 B ← UART1 ← ESP32 B
```

양방향 가능. 무선 중계는 표준 Mesh Relay가 담당하며, 수신한 이벤트를 앱에서 재발행하지 않는다. 이 단계에는 Periodic Task·속도/온도 수치·SharedState·대시보드 동기화·ACK/재시도가 없다.

## 1. 빌드부터

ESP-IDF **v5.5.5**, ESP32-S3, 현재 기본 flash 설정은 **16MB**다. 실제 보드의 flash 용량과 GPIO를 확인하고 필요한 경우 `menuconfig`에서 조정한다.

```sh
source /path/to/esp-idf-v5.5.5/export.sh
cd integration/esp32-s3
idf.py -DIDF_TARGET=esp32s3 build
```

이 저장소와 ESP-IDF만 필요하다. 외부 `esp-ble/layers/` 또는 `communication-module/`에 의존하지 않는다. `example_init`은 `main/idf_component.yml`에 지정된 **설치된 IDF 안의** Bluetooth 초기화 helper다. `sdkconfig.defaults`는 신규 설정의 기본값이며 이미 생성된 `sdkconfig`를 자동 덮어쓰지 않는다.

이 문서의 명령은 build까지만 수행한다. Flash 전에 보드/포트를 식별하고 기존 펌웨어 및 Mesh 구성의 변경 영향을 확인한다. **NVS 자동 erase나 factory reset 코드는 없다.** NVS 초기화가 실패하면 기존 설정을 유지하고 중지한다.

## 2. 배선

전원을 끄고 실제 보드 실크·회로·사용 중인 핀을 먼저 확인한다. 3.3V UART, 115200 baud, 8-N-1, flow control 없음.

| STM32 NUCLEO-F411RE | ESP32-S3 GPIO |
| --- | --- |
| PA9 / USART1 TX | GPIO18 / UART1 RX |
| PA10 / USART1 RX | GPIO17 / UART1 TX |
| GND | GND |

각 보드를 USB로 별도 전원 공급할 때 전원 레일(3V3/5V)을 서로 연결하지 않는다. GPIO 번호는 물리 헤더 순번이 아니다. 데이터 UART1에는 **바이너리 ID 1바이트만** 흐른다. 로그/명령은 기본 UART0 콘솔 및 SDK의 secondary USB Serial/JTAG 출력과 분리한다. 콘솔 입력 포트는 실제 연결한 보드에서 확인한다.

## 3. Mesh 설정

새 Composition은 기존 Generic OnOff Server/Client에 Vendor Model을 추가한다. 기존 Layer 7의 provisioning을 그대로 재사용할 수 있다고 가정하지 않는다. 설정 삭제/재provisioning은 해당 보드의 변경 승인을 받은 후 별도 진행한다.

시험용 Provisioner에서 각 노드에:

1. 같은 시험 네트워크에 provision하고 **Composition Data를 다시 읽는다**.
2. 같은 AppKey를 Add하고 Vendor Model에 Bind한다. AppKey/NetKey의 **실제 index**를 사용한다.
3. Vendor Publication과 Subscription을 아래 표대로 설정한다.
4. 콘솔 `status`에서 `event_ready=1`, `sub_C001=1`을 각각 확인한다.
5. OnOff 회귀 검사를 할 때만 SIG OnOff 모델에 Bind 및 C000 설정도 한다.

| 항목 | 이벤트 Vendor Model | OnOff 회귀 모델 |
| --- | --- | --- |
| Company ID / Model ID | `02E5 / 0001` | SIG Server `1000`, Client `1001` |
| opcode | `ESP_BLE_MESH_MODEL_OP_3(0x20, 0x02E5)` | 기존 Generic OnOff |
| Publication | `C001`, TTL **7**, period **0**, retransmit **0** | Client → `C000` |
| Subscription | `C001` | Server ← `C000` |

`02E5`는 Espressif 예제의 Company ID를 빌린 **폐쇄형 학습 시험용**이다. 팀의 할당 ID나 제품 배포 규격이 아니다. 상용화/외부 배포 규약은 별도 결정한다.

`event_ready`는 자기 주소, 유효한 AppKey↔NetKey index 매핑, Vendor Bind, Publication/C001/TTL7/period0/retransmit0가 준비됐다는 뜻이다. Subscription은 별도로 `sub_C001`로 표시하며 상대 노드 설정/수신 성공을 보장하지 않는다. key 삭제/unbind/publication 변경 후에도 readiness를 재계산한다.

키 **값**은 SDK가 관리한다. 앱은 AppKey Add callback에서 얻은 index 매핑만 별도 NVS namespace `bsg_bridge`에 보존한다. 재부팅 시 SDK의 실제 키·모델 상태와 대조한다. 유효한 매핑이 없으면 임의 NetKey를 추측하지 않고 송신을 막는다. 저장 실패 때는 현재 메모리에 남은 매핑으로 동작할 수 있지만, 재부팅 후 준비 상태는 보장하지 않는다. 이 경우 같은 키의 AppKey Add를 다시 구성하고 로그를 확인한다. 자동 초기화/삭제로 복구하지 않는다.

이벤트 송신은 publication 설정을 확인한 뒤, payload/context를 복사하는 `esp_ble_mesh_server_model_send_msg`로 group C001에 명시적으로 한 번 보낸다. 공용 publication 버퍼를 덮어쓰지 않으며 periodic publish callback은 이벤트를 만들지 않는다. 네트워크 계층의 설정된 송신/Relay 반복은 앱의 재시도와 별개다.

Relay 기본값은 **disabled**다. 세 노드 중계 시험에서만 중간 노드 Relay를 명시적으로 켜고 직접 경로를 통제한다. 가까이 있는 세 노드의 수신만으로 multi-hop 성공이라 판단하지 않는다.

## 4. Task와 관찰

| 실행 주체 | 앱 priority | 역할 |
| --- | --- | --- |
| UART RX Task | 4 | UART1 입력 검사 → FIFO 값 복사 |
| Mesh callback | SDK 관리 | payload/source 검사 → FIFO 값 복사, UART 대기 없음 |
| Bridge Task | 5 | FIFO에서 꺼내 한 번 전송, 처리 후 1 tick 양보 |
| Console Task | 2 | 상태 조회, OnOff/송신 출력 세기 시험 |

이 숫자는 앱 내부 상대 우선순위다. Bluetooth 시스템 Task를 낮추거나 모든 Task보다 무조건 높게 설정하지 않는다. 이벤트 전용 단계이므로 낮은 우선순위의 periodic task는 아직 없다. FIFO는 정적 32건, 큐·통계는 공통 critical section으로 보호하고, 일이 없으면 task notification/driver queue에서 대기한다.

콘솔 명령: `status`, `on`, `off`, `on-unack`, `off-unack`, `tx-low`, `tx-normal`. `factory-reset`은 parser가 인식해도 실행은 거부한다. OnOff의 앱 state/TID는 이 새 펌웨어에서 volatile이며, 재부팅 직후 같은 TID의 회귀 시험은 Mesh transaction window가 지난 뒤 진행한다.

`status` 해석:

- `uart_rx valid/noop/invalid`: STM32 쪽 입력. `hw_errors`는 FIFO/버퍼 초과·frame/parity 오류로 입력을 비운 횟수이며 정확한 손실 바이트 수가 아니다.
- `MESH_RX valid/invalid/self`: valid에는 self/full로 제외된 것도 포함한다.
- `not_ready`: UART 입력 시점 또는 처리 시점의 Mesh 미준비 drop.
- `MESH_TX`, `UART_TX`의 `accepted/failed/full/expired`: 방향별 결과.
- `MESH_STACK complete_ok/failed`: 비동기 SDK 전송 완료 결과. **상대 ACK가 아니다.**
- `TX ... source=... id=...`: 원격 source는 로그에만 보존한다. STM32 한 바이트 프로토콜에 출처는 실리지 않는다.

UART1 TX는 Bridge Task 한 곳만 소유한다. `uart_tx_chars`로 1바이트를 하드웨어 FIFO에 복사하고 `uart_wait_tx_done`의 대기 예산을 10ms로 제한한다. 다른 writer를 추가하면 내부 mutex 대기가 생길 수 있으므로 금지한다. 스케줄링까지 포함한 hard real-time 10ms 전달 보장은 아니다. timeout 때 이미 하드웨어로 보낸 바이트를 취소할 수 없으며 재전송하지 않는다.

## 5. 실물 검증 순서와 한계

[VERIFICATION.md](VERIFICATION.md)의 미확인 항목을 순서대로 진행한다. 처음에는 하나씩 저속으로 입력하고 상대 STM32 RX 카운터·ID·출력을 확인한 뒤 다음 이벤트를 보낸다. `message_debug_inject`는 기존 STM32에서 **remote 처리 경로**이므로 이를 바꿨다고 자동 UART TX가 발생하는 것은 아니다. 로컬 버튼/센서 또는 승인된 전용 입력 장치로 송신을 시험한다.

- 기존 STM32 수신은 **한 건 pending slot**이다. 추가 입력을 버릴 수 있으며 ESP32의 32건 큐가 이 한계를 해결하지 않는다.
- CRC, source, event sequence, ACK가 없다. 유효 ID 사이의 비트 오류, 손실/장기 중복/최대 지연을 보장하지 않는다.
- 큐의 1000ms 만료는 **앱이 UART 바이트를 읽거나 Mesh callback을 처리한 시각부터** 계산한다. 센서 측정 시각이나 드라이버/무선 내부 대기 시간까지 포함하지 않는다. 설정 전 데이터는 입력 처리 시점의 readiness로 거부하므로 설정 중 입력하지 않는 것이 시험 원칙이다.
- 설정 변경과 전송이 겹치면 SDK가 뒤늦게 실패할 수 있다. `accepted`와 비동기 실패를 함께 본다. 자동 재시도/보류 후 재생은 없다.
- 낙차/SOS는 기존 latch 정책, 후방 이벤트는 상태 변화 정책을 유지한다. 사람을 실제로 넘어뜨리는 시험은 하지 않는다.

## 코드 출처

학습 공간 `esp-ble/layers/layer-7/`의 Composition/OnOff 흐름과 콘솔 parser를 바탕으로 새 독립 프로젝트를 작성했다. parser·SDK 설정 템플릿·`idf_component.yml`은 그곳에서 가져왔다. 원본 Layer 7 파일은 변경하지 않았고 build/logs는 가져오지 않았다. Vendor API와 버퍼 수명은 설치된 ESP-IDF v5.5.5의 `examples/bluetooth/esp_ble_mesh/vendor_models/vendor_server` 및 `components/bt/esp_ble_mesh/api/core/esp_ble_mesh_networking_api.c`, `btc/btc_ble_mesh_prov.c`를 대조했다. SDK helper는 원래 SDK 라이선스를 유지한 외부 빌드 의존성이다.

---

# 공통 API 상세

[저장소 시작](../../README.md) · [ESP32 사용법](../../integration/esp32-s3/README.md)

STM32의 센서 판정 결과를 전달하는 **1차 이벤트 전용** 코드다. HAL·ESP-IDF·동적 할당 없이 C11로 빌드한다. 속도 이동평균·센서 읽기·주기 전송은 이 코드의 책임이 아니다.

## 계약

ID의 유일한 원본은 [message_type.h](../../common/protocol/message_type.h)다. CMake include 경로로 양쪽에서 참조하며 enum을 복제하지 않는다.

| 이벤트 | UART | Mesh application payload |
| --- | --- | --- |
| 감속 | `10` | `01 10` |
| 가속 | `11` | `01 11` |
| 안전·응원 | `12` | `01 12` |
| 정지 | `13` | `01 13` |
| 후방 안전 | `20` | `01 20` |
| 후방 경고 | `21` | `01 21` |
| 낙차 | `30` | `01 30` |
| SOS | `31` | `01 31` |

위 값은 16진수다. UART에는 한 바이트만 보낸다. `"10"` 문자열이나 줄바꿈을 보내지 않는다. Mesh에서는 opcode 뒤 정확히 2바이트, `[version=1, ID]`를 사용한다. `sizeof(message_type_t)`나 `sizeof(event_job_t)`로 전송하지 않는다.

UART `00`은 no-op. 나머지 미정의 값은 invalid. Mesh payload의 `00`, 틀린 version/길이도 invalid다. 실패한 decode는 출력 ID를 변경하지 않는다.

## API 읽는 순서

1. [event_protocol.h](../../common/protocol/event_protocol.h): `event_id_valid`, `event_encode`, `event_decode`.
2. [event_bridge.h](../../common/protocol/event_bridge.h): 입력을 값 복사해 넣고, 만료·준비 상태를 검사하며 꺼내는 고정 FIFO.
3. [event_bridge.c](../../common/protocol/event_bridge.c): 실제 정책. [tests](../../common/protocol/tests/)에서 공개 API의 예제를 확인한다.

```c
event_bridge_t bridge;
event_bridge_init(&bridge);
event_bridge_uart(&bridge, MSG_STOP_REQUEST, now_ms, mesh_ready);
// 반대 방향: event_bridge_mesh(..., source, own_address, now_ms)

event_job_t job;
if (event_bridge_next(&bridge, now_ms, mesh_ready, &job) == EVENT_OK) {
    bool accepted = event_job_send(&job, &transport);
    event_bridge_complete(&bridge, job.direction, accepted);
}
```

`transport.mesh`는 2바이트, `transport.uart`는 1바이트를 받는다. 포인터는 호출 중에만 유효하므로 callback이 반환하기 전에 소비/복사해야 한다. 테스트는 이 외부 전송 경계와 시간을 주입한다.

### 대기열 정책

- UART→Mesh, Mesh→UART가 같은 **32건 FIFO**를 쓴다. 내부 구현은 고정 배열 ring buffer다. 두 방향을 합친 용량이며 각각 32건이 아니다.
- full이면 새 입력을 거부한다. 기존 항목을 덮어쓰지 않는다.
- 같은 ID가 반복되어도 별도 이벤트다. source가 자기 primary address인 Mesh 입력만 제외한다.
- `now - received >= 1000ms`면 만료, 시계가 뒤로 간 입력도 만료 취급한다. 로컬 64비트 단조 시각만 쓴다.
- UART 입력 때 Mesh 미준비이면 저장하지 않는다. 꺼낼 때도 미준비이면 버린다.
- `event_bridge_next`는 한 건을 **소비**한다. 만료/미준비 결과도 이미 소비된 것이다. 유효 작업의 전송 API는 한 번만 호출하고 실패해도 재삽입하지 않는다.
- Mesh 입력은 UART로만 나간다. 원격 수신을 다시 Mesh로 발행하지 않는다.

이 C 객체 자체는 thread-safe가 아니다. ESP32 [bridge_runtime.c](../../integration/esp32-s3/main/bridge_runtime.c)가 **모든 큐·통계 접근을 같은 `portMUX`로 보호**한다. UART Task와 Mesh callback은 짧은 critical section 안에서 값 복사만 하고, Bridge Task는 잠금을 푼 뒤 전송한다. Task notification은 깨우기 신호일 뿐이며 이벤트 저장은 FIFO가 담당한다. 다른 RTOS에 포팅할 때도 같은 직렬화가 필요하다.

`accepted`는 로컬 전송 API의 성공이다. 상대 STM32의 수신/출력/ACK를 뜻하지 않는다. 카운터는 부팅부터 누적되는 `uint32_t`이며 영구 저장하지 않는다.

## 호스트 검사

저장소 루트에서:

```sh
cmake -S common/protocol -B common/protocol/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build common/protocol/build/debug
ctest --test-dir common/protocol/build/debug --output-on-failure
```

Release는 별도 build 디렉터리와 `-DCMAKE_BUILD_TYPE=Release`, sanitizer는 `-DENABLE_SANITIZERS=ON`을 사용한다. GCC/Clang 계열용이며 테스트는 Release에서도 비활성화되지 않는 CHECK를 쓴다. 콘솔 parser 회귀 검사를 포함하지만 실제 reset 명령 실행은 펌웨어에서 거부한다.

호스트 PASS는 UART 드라이버, RTOS 동시 실행, Mesh 설정/무선 수신 또는 센서 판정의 검증을 대신하지 않는다.
