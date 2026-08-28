> 이관 원문: `layers/layer-8/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 8 — STM32 UART ↔ Bluetooth Mesh

현재 **D10/PB6 버튼 + D8/PA9 USART1 → D6 → 76/B6**의 반복 검증은
[빠른 검증 도구](FAST_CHECK.md)를 사용한다: `bash fast-check.sh`.
현재 버튼 테스트와 USART2 USB 진단 복사본의 설명은 위 문서를 따른다.

실제 UART 핀 매핑·통신 설정·GPIO18 입력 상태는
[읽기 전용 UART 진단](UART_DIAGNOSTICS.md)의 `status` 출력으로 확인한다.

**STM32가 버튼을 해석하고, ESP32는 그 메시지를 같은 그룹의 다른 ESP32들에게 전달한다.**
Layer 7의 표준 Bluetooth Mesh에 UART 이벤트 송수신을 추가한 독립 ESP-IDF 프로젝트다.
Layer 7과 STM32 쪽 소스는 수정하지 않는다. 모든 ESP32에는 같은 Layer 8 이미지를 사용한다.

```text
STM32 A 버튼 → USART1 TX → ESP32 A UART1 RX
                                  ↓ Mesh 이벤트 그룹 0xC001
STM32 B 출력 ← USART1 RX ← ESP32 B UART1 TX
STM32 C 출력 ← USART1 RX ← ESP32 C UART1 TX
```

B/C도 자기 STM32에서 이벤트를 받으면 같은 방식으로 보낸다. 여기서 'broadcast'는
**같은 Mesh 네트워크·AppKey를 사용하고 C001을 구독한 노드들에게 보내는 group 전송**이다.
주변의 모든 BLE 기기에게 보내거나, 모든 수신자의 도착을 보장하는 방식은 아니다.

현재 검증 범위는 [VERIFICATION.md](VERIFICATION.md)를 따른다. 빌드는 Flash/무선 수신 증거가 아니다.

현재 단일 버튼 실물 시험은 **STM32 D10/PB6 버튼 → USART1 D8/PA9 TX → ESP32 GPIO18** 구성이다. USART2는 ST-LINK USB 진단 복사본에만 사용한다. 배선과 자동 관찰 방법은 [빠른 버튼 → Mesh 검증](FAST_CHECK.md), 실제로 확인된 경계는 [STM32 버튼 시험 기록](../../firmware/stm32/BUTTON_UART_TEST.md)을 따른다. USB 진단 복사본 수신만으로 D8에서 ESP32까지의 전기적 전달을 입증하지 않는다.

## 1. 배선과 데이터

전원을 끈 상태에서 실제 보드 핀을 확인하고 연결한다. GPIO 번호는 헤더의 순번이 아니다.

| STM32 NUCLEO-F411RE | ESP32-S3 | 역할 |
| --- | --- | --- |
| PA9 / USART1 TX | GPIO18 / UART1 RX | STM32 → ESP32 |
| PA10 / USART1 RX | GPIO17 / UART1 TX | ESP32 → STM32 |
| GND | GND | 공통 기준 전압 |

3.3V UART, **115200 baud / 8-N-1 / flow control 없음**.
두 보드를 각각 USB로 전원 공급할 때 3V3/5V 전원 레일은 서로 연결하지 않는다.
ESP32 USB Serial/JTAG는 로그·명령용이고, UART1은 **이벤트 ID 1바이트 전용**이다.
STM32 ST-Link USB 콘솔과 USART1도 다른 통신 경로다.

| 버튼/이벤트 | UART 바이트 | Mesh payload |
| --- | --- | --- |
| BTN1 / 감속 | `10` | `01 10` |
| BTN2 / 가속 | `11` | `01 11` |
| BTN3 / 안전·응원 | `12` | `01 12` |
| BTN4 / 정지 | `13` | `01 13` |
| 후방 안전 / 경고 | `20` / `21` | `01 20` / `01 21` |
| 낙차 / SOS | `30` / `31` | `01 30` / `01 31` |

모두 16진수다. `13`은 문자열 `"13"`이 아니라 바이너리 한 바이트 `0x13`이다.
Mesh에서는 opcode 뒤에 `[version=1, ID]` 두 바이트를 넣고, 받는 ESP32는 ID만 UART로 출력한다.
현재 STM32 소스의 BTN1~4는 각각 PB5/PB10/PA8/PC7이다. 보드의 USER/RESET 버튼과 혼동하지 않는다.
센서 판정·디바운싱·음성/LED 동작은 STM32 책임이며 Layer 8에서 다시 해석하지 않는다.

## 2. 빌드 — 보드에는 쓰지 않음

검증 SDK는 ESP-IDF v5.5.5, 대상은 ESP32-S3, 기본 flash 설정은 16MB다.
설치 위치가 다르면 `export.sh` 경로만 바꾼다.

```bash
source /Users/kafka/esp/esp-idf-v5.5.5/export.sh
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-8
idf.py build
bash test-host.sh
```

결과 앱: `build/esp32s3_layer_8.bin`, `build/esp32s3_layer_8.elf`.
`test-host.sh`는 Debug / Release / ASan+UBSan에서 이벤트 변환·큐·콘솔 parser·송수신 경로를 검사한다.
`common/`과 `main/`만 로컬 소스로 사용하므로 `stm32-project`나 `layers/layer-7`가 빌드에 필요하지 않다.
외부 의존성은 설치한 ESP-IDF와 그 안의 `example_init` helper다.

## 3. Flash와 USB 콘솔 — 대상 확인·승인 후 별도 실행

먼저 USB 포트와 대상 보드를 다시 식별한다. 아래 명령은 **이전 앱을 교체**하므로
원래 firmware와 Mesh 설정의 변경 영향을 확인한 뒤 실행한다.

```bash
# 위의 ESP-IDF 환경과 Layer 8 작업 디렉터리에서 실행
PORT=/dev/cu.usbmodemXXXX  # 현재 확인한 ESP32 포트로 변경
idf.py -p "$PORT" flash
idf.py -p "$PORT" monitor --no-reset
```

다른 ESP32도 같은 이미지로 설치한다. 자동 전체 erase/factory-reset은 하지 않는다.
두 보드를 볼 때는 터미널을 두 개 열어 서로 다른 포트를 지정한다. 같은 포트를 중복해서 열지 않는다.
ESP32-S3의 **native USB Serial/JTAG 포트**를 사용한다. 별도 USB-UART 포트는 이 콘솔이 아니다.
모니터 종료: `Ctrl+]`. Flash 뒤 다음 출력과 실제 `status` 응답을 확인한다.

```text
Project name: esp32s3_layer_8
[LAYER-8] APP_STARTED
```

명령은 `status`, `on`, `off`, `on-unack`, `off-unack`, `tx-low`, `tx-normal`이다.
`on/off`는 기존 OnOff 회귀 시험용이며 STM32 버튼 이벤트 전송을 대신하지 않는다.
`factory-reset`은 parser가 구분하지만 실행은 거부한다. `status`는 설정·큐·송수신 카운터를 출력한다.

## 4. Mesh 설정 — C000 OnOff와 C001 이벤트는 다름

Layer 8은 기존 SIG OnOff 모델에 **Vendor Model**을 추가하여 Composition이 달라진다.
Layer 7의 provisioning/앱 캐시가 그대로 맞는다고 가정하지 않는다.
새 Composition을 확인하고 필요한 경우 **설정 삭제·재등록을 별도 승인 후** 진행한다.

Provisioner에서 각 노드에 같은 AppKey를 Add한 뒤:

| 항목 | 설정 |
| --- | --- |
| Vendor Company ID / Model ID | `0x02E5` / `0x0001` |
| Vendor opcode | `ESP_BLE_MESH_MODEL_OP_3(0x20, 0x02E5)` |
| Model AppKey Bind | 실제 추가한 AppKey index |
| Vendor Publication | `0xC001`, TTL `7`, period `0`, retransmit `0` |
| Vendor Subscription | `0xC001` |

`status`에서 각 노드의 **`event_ready=1`, `sub_C001=1`**을 확인한다.
주소는 서로 달라야 한다. AppKey↔NetKey index는 실제 설정에서 얻으며 `0`이라고 추측하지 않는다.
`NO_KEY_INDEX_MAP`이면 SDK 설정과 index 매핑을 확인하고 필요한 AppKey Add를 다시 수행한다.
NVS 오류 시 기존 설정을 지우지 않고 시작을 중단한다.

OnOff 회귀 시험이 필요할 때만 SIG Server/Client에 별도로 AppKey Bind와 C000 설정을 한다.
`0x02E5`는 Espressif 예제의 학습용 ID이며 제품 배포용 할당 ID가 아니다.
Relay 기본값은 disabled지만 저장된 설정이 있으면 그 값을 복원한다. 직접 경로를 차단한
Relay OFF/ON 비교 없이는 다중 홉 중계 성공이라고 하지 않는다.

## 5. 버튼 한 번 시험

아래는 **기대 로그 예시이지 실제 수신 기록이 아니다**. BTN4 정지 요청의 경우:

```text
송신 ESP32: UART_RX id=0x13 result=queued
송신 ESP32: MESH_TX id=0x13 source=... api=accepted
수신 ESP32: MESH_RX source=... id=0x13 result=queued
수신 ESP32: UART_TX id=0x13 source=... api=accepted
```

1. 각 노드에서 `status`를 기록하고, 배선된 STM32의 버튼을 한 번 누른다.
2. 송신 ESP32의 UART_RX/MESH_TX와 **상대 ESP32 각각의** MESH_RX/UART_TX를 확인한다.
3. 상대 STM32가 있다면 `uart_debug_rx_count`, `uart_debug_last_received`와 실제 출력을 확인한다.
4. 한 건이 확인되면 다음 버튼을 누른다. 같은 버튼을 다시 눌러도 새 입력으로 보낸다.

STM32 1대 + ESP32 2대 구성에서는 버튼 → 상대 ESP32 수신까지 시험할 수 있다.
수신 ESP32의 UART_TX 로그만으로 상대 STM32가 실제 받았다고 판단하지 않는다.
로그는 서로 다른 Task에서 나오므로 인접 로그의 표시 순서만으로 실행 순서를 단정하지 않는다.

## 6. 큐와 실패 처리

- 두 방향이 공유하는 정적 FIFO는 **32건**이다. 가득 차면 새 입력을 버린다.
- 앱 입력 후 **1000ms 이상** 지난 작업은 버린다. 센서 측정 시각 기준은 아니다.
- Mesh 미준비 상태의 UART 입력은 즉시 버리며, 설정 뒤 몰아서 재생하지 않는다.
- UART `00`은 no-op, 미정의 ID·잘못된 길이/version은 거부한다.
- 자기 Mesh 메시지는 UART로 되돌리지 않고, 원격 수신은 UART로만 보낸다.
- 앱 재시도·상대 ACK·source별 수치 동기화는 없다. 무선 중계/네트워크 반복은 Mesh 스택 책임이다.
- `api=accepted`와 `MESH_STACK complete_ok`는 로컬 처리 결과이며 상대 수신 보장이 아니다.
- `status`의 invalid/noop/not_ready/full/expired/hw_errors/failed 카운터로 실패 단계를 구분한다.
- 이 로그 구성은 저속 버튼 시험용이다. 지속 고속 입력의 처리율·Task stack·시간 한계는 실물에서 별도 검사한다.

## 코드 읽는 순서와 출처

1. [main/main.c](../../../code/layers/layer-8/main/main.c): NVS → UART 입력 처리 → Bluetooth/Mesh → USB 콘솔.
2. [main/bridge_runtime.c](../../../code/layers/layer-8/main/bridge_runtime.c): UART 입력 Task, 공유 큐, 실제 송신 Task.
3. [main/mesh_node.c](../../../code/layers/layer-8/main/mesh_node.c): Provisioning/설정, Vendor 송수신, 기존 OnOff 모델.
4. [common/event_protocol.c](../../../code/layers/layer-8/common/event_protocol.c): 1바이트 ID ↔ 2바이트 Mesh payload.
5. [common/event_bridge.c](../../../code/layers/layer-8/common/event_bridge.c): 큐·만료·방향·자기 메시지 제외.

`stm32-project/integration/esp32-s3/main`과 `stm32-project/common/protocol`의
2026-08-28 구현을 독립 학습용으로 복사했다. `common/message_type.h`도 동일 ID의 스냅샷이다.
원본과 자동 동기화되지 않으므로 계약을 바꿀 때 양쪽을 확인한다.
Layer 8의 변경은 로컬 의존성, 앱/노드 이름, USB primary console, 단계별 로그, 경로 검사다.
가져온 소스의 `off` 명령 함수명 오타도 Layer 8에서만 수정했다.
SDK helper는 ESP-IDF 원본의 라이선스를 따른다.

[Layers 목록](../README.md) · [Layer 로드맵](../../02-learning/LAYER-ROADMAP.md)
