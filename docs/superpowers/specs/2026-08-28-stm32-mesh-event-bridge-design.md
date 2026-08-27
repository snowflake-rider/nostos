# STM32 ↔ ESP32 Mesh — 1차 이벤트 통합 설계

[저장소 시작](../../../README.md) · [현재 STM32 통합 기록](../../../integration/README.md)

작성일: 2026-08-28. 기준 STM32 revision: `3c67d2f`.

상태: **이벤트 전달 먼저 → 센서 수치 공유 후속**이라는 방향은 사용자 승인. 아래 상세 계약은 구현 전 검토안이며, 연결 코드·빌드·실물 통합 완료를 뜻하지 않는다.

## 1. 목표와 범위

각 라이더의 STM32가 센서 읽기·판정·로컬 출력을 담당하고 ESP32가 Mesh 통신을 담당한다. STM32와 ESP32는 서로 다른 펌웨어로 빌드한다.

```text
버튼·센서 → STM32 A → USART1 → ESP32 A
                                  │ Bluetooth Mesh
LED·음성 ← STM32 B ← USART1 ← ESP32 B
```

모든 노드가 같은 송수신 기능을 가진다. B가 자기 센서에서 생성한 메시지는 반대 방향으로도 공유한다. 중간 노드의 무선 Relay는 Mesh 스택의 기능이며 STM32에서 수신 이벤트를 재발행하지 않는다.

1차 완료 목표는 기존 8종 이벤트가 상대 STM32까지 도달하는 경로를 관찰하는 것이다. 거리·가속도 등 수치, Periodic Task, SharedState, 대시보드 통합, 앱 수준 ACK·재전송은 후속 범위다. 센서 판정이나 알림 임계값을 이번 통합에서 바꾸지 않는다.

현재 STM32에는 MPU6050과 HC-SR04 처리가 있다. 속도·온도 센서까지 이미 통합됐다고 가정하지 않는다. 이전 설계의 ESP32 직접 센서 읽기 대신, 이번 통합의 센서 생산자는 STM32다.

## 2. 기존 코드와 보존 경계

- [message_type.h](../../../integration/stm32/MyApp/common/message_type.h): 공통 이벤트 ID의 유일한 원본. HAL에 의존하지 않으므로 ESP32/호스트 코드에서 같은 헤더를 참조한다.
- [uart_service.c](../../../integration/stm32/MyApp/service/uart_service.c): USART1으로 ID 한 바이트를 송수신한다. 현재 수신 대기 공간은 한 건이다.
- [message_router.c](../../../integration/stm32/MyApp/service/message_router.c): 로컬 이벤트는 로컬 처리 후 송신하고, 원격 이벤트는 로컬 처리만 한다.
- ESP32 기반은 별도 학습 공간의 `esp-ble/layers/layer-7/`와 ESP-IDF v5.5.5다. 기존 Layer 7 파일은 변경하지 않는다. build/logs는 복사하지 않고 필요한 소스·설정 템플릿만 새 프로젝트로 가져오며 출처를 README에 기록한다.
- 이 단계에서는 STM32의 C/H, `.ioc`, 빌드 설정, 센서·출력 동작을 유지한다. 기존 별도 로컬 저장소의 미커밋 수정과 `.ioc`도 가져오거나 덮어쓰지 않는다.

예정 위치:

```text
common/protocol/                 # HAL/ESP-IDF 없는 이벤트 검사·Mesh payload codec·호스트 검사
integration/stm32/               # 기존 STM32 프로젝트: 1차 소스 변경 없음
integration/esp32-s3/            # 새 ESP-IDF 프로젝트: UART ↔ Mesh bridge
```

새 ESP32 프로젝트는 이 Git 저장소만으로 빌드할 수 있어야 한다. 빌드 시 외부 `esp-ble/layers/` 또는 기존 `communication-module/` 경로에 의존하지 않는다. 이전 통신 모듈의 버튼 1/2/3 API는 아래 8종 ID와 다르므로 무리하게 그대로 연결하지 않는다.

## 3. 유지할 UART 계약

USART1, 115200 baud, 8-N-1, flow control 없음. enum 객체나 ASCII 문자열이 아니라 **ID 한 바이트**다. 줄바꿈·종료 문자·센서 수치를 덧붙이지 않는다.

| ID | 의미 |
| --- | --- |
| `0x10` | 감속 요청 |
| `0x11` | 가속 요청 |
| `0x12` | 안전·응원 |
| `0x13` | 정지 요청 |
| `0x20` | 후방 안전 상태 |
| `0x21` | 후방 경고 |
| `0x30` | 낙차 감지 |
| `0x31` | SOS |

`0x00`은 무동작으로 소비한다. `0xFF`와 나머지 값은 거부하고 invalid 카운터에 기록한다. ESP32는 이벤트의 의미를 변경하거나 자체적으로 새로운 센서 판정을 만들지 않는다.

ESP32-S3-DevKitC-1용 기본 연결안:

| STM32 | 연결 | ESP32-S3 |
| --- | --- | --- |
| PA9 / USART1 TX | → | GPIO18 / UART1 RX |
| PA10 / USART1 RX | ← | GPIO17 / UART1 TX |
| GND | ↔ | GND |

GPIO 번호는 헤더의 물리적 핀 순번과 다르다. 실제 보드 실크·회로·점유 핀을 대조한 후 배선한다. 각 보드를 USB로 전원 공급할 때 3V3/5V 전원 레일은 연결하지 않는다. STM32용 데이터 UART1에는 로그나 관리 명령을 섞지 않는다. ESP32 UART0/USB 콘솔은 디버깅용으로 분리한다. [공식 S3 DevKitC-1 핀 설명](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html)

## 4. Mesh 계약 — 1차 시험용

Generic OnOff에 이벤트 ID를 끼워 넣지 않는다. Layer 7의 OnOff 모델은 회귀 검증용으로 유지하고, 같은 Element에 송수신 가능한 Vendor Model 하나를 추가한다. 모든 ESP32에 같은 구성을 사용한다. Sensor/Vendor Model은 공식 예제로 제공된다. [ESP-IDF 예제 목록](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/esp-ble-mesh/ble-mesh-index.html#examples)

검토안의 시험용 설정은 다음과 같다.

- Vendor Company ID `0x02E5`, Model ID `0x0001`, 이벤트 opcode `ESP_BLE_MESH_MODEL_OP_3(0x20, 0x02E5)`.
- `0x02E5`는 설치된 Espressif 예제의 ID를 사용하는 폐쇄된 학습용 설정이다. 우리 팀에 할당된 Company ID나 제품 배포용 규격으로 표현하지 않는다. 상용 배포 전에는 식별자·모델 규약을 별도 확정한다.
- 이벤트 group `0xC001`, TTL 7. 기존 OnOff group `0xC000`과 구분한다.
- 각 Vendor Model에 같은 AppKey Bind, Publication `0xC001`, Subscription `0xC001` 설정. 주기 Publication과 앱 수준 Publication 재전송은 끈다.
- Provisioning, AppKey, Vendor Model Bind/Publication이 준비되지 않으면 UART 이벤트를 무선 송신하지 않고 사유를 기록한다. 설정 전에 눌렀던 버튼을 나중에 재생하지 않는다.
- 이벤트는 unacknowledged 전송이다. Mesh API의 접수 성공은 상대 앱의 처리 완료가 아니다.

opcode 뒤 application payload는 정확히 두 바이트다.

| Offset | 크기 | 내용 |
| --- | --- | --- |
| 0 | 1 byte | version = `0x01` |
| 1 | 1 byte | 위 표의 유효 이벤트 ID |

길이·version·ID가 맞지 않으면 거부한다. 원래 source는 Mesh 수신 context에서 얻어 로그에 남긴다. 현재 STM32의 한 바이트 인터페이스로 source까지 전달할 수는 없다. 이번 단계는 출처별 대시보드를 완성하는 단계가 아니다.

수신한 이벤트는 UART로만 전달하며 새 Mesh 메시지로 재발행하지 않는다. source가 자기 primary address인 자기 발행 메시지는 UART로 되돌려 보내지 않는다. 같은 ID를 다시 누른 것은 새 입력이므로 ID 값만 보고 중복 제거하지 않는다. Mesh 스택의 중복 처리를 애플리케이션의 영구적인 exactly-once 보장으로 표현하지 않는다.

## 5. 실행 흐름·대기열·오류

- UART 수신 Task는 입력 바이트를 읽어 검증 후 값 복사로 작업을 넘긴다.
- Mesh callback은 길이·형식·source를 확인하고 필요한 값만 복사한다. UART 송신을 기다리지 않으며 callback 포인터를 저장하지 않는다.
- Bridge Task가 앱의 송신 작업과 카운터를 소유한다. 정적 RTOS-safe FIFO 32건으로 UART→Mesh / Mesh→UART 작업을 처리한다. 같은 ID의 서로 다른 입력은 별도 작업이다.
- 큐가 가득 차면 새 작업을 버리고 방향별 drop 카운터를 증가시킨다. 무한 대기나 오래된 작업 덮어쓰기를 하지 않는다.
- 각 작업은 수신한 로컬 단조 시각으로부터 1초가 지나면 전송하지 않고 만료로 기록한다. 원격 노드 시각을 빼서 계산하지 않는다.
- 작업 처리 시에도 Mesh 준비 상태를 재확인한다. 준비 상태 상실, 잘못된 입력, 큐 초과, 만료, 전송 API 실패는 구분해서 기록한다.
- 유효 작업마다 전송 API를 한 번 시도한다. 이번 단계에서는 애플리케이션 자동 재시도를 하지 않는다. 성공 접수 후 같은 작업을 다시 송신하지 않는다. Mesh 스택 자체의 네트워크 전송 동작은 별도다.
- UART 출력은 한 바이트이며 대기 상한 10ms를 둔다. 드라이버에 넘기는 데이터의 수명은 호출 종료와 분리해 안전하게 관리한다.
- 일이 없으면 Task는 큐/드라이버에서 blocking한다. Bluetooth 시스템 Task를 굶기지 않는다. Periodic Task는 이번 범위에 없다.

최소 관찰 항목: UART 유효 RX, invalid/no-op, Mesh TX 접수/실패, Mesh RX, 자기 메시지 제외, UART TX 접수/실패, 큐 초과, 작업 만료. UART TX 접수 카운터를 STM32 RX 카운터와 동일시하지 않는다.

## 6. 전달 보장과 안전 경계

현재 STM32는 수신 대기 메시지가 있으면 다음 바이트를 버릴 수 있다. 또한 UART에는 CRC·source·ACK가 없고, 손상된 바이트가 다른 유효 ID로 바뀌면 검출할 수 없다. ESP32 큐만 늘려도 이 제약은 사라지지 않는다.

따라서 1차는 저속 이벤트 경로 검증용 best-effort 프로토타입이다. 무손실·최대 지연·수신 확인·안전 기능의 신뢰성을 보장하지 않는다. 후속 프레임 프로토콜에서 흐름 제어, CRC, 사건 식별, ACK/재시도 및 수신 큐를 함께 설계한다.

낙차 이벤트는 디버거 주입 또는 안전한 기록·시험 장치로 검증한다. 사람이 넘어지는 시험을 전제로 하지 않는다. 기존 센서 판정의 정확도를 통신 성공으로 대신 검증하지 않는다.

## 7. 설정 변경과 검증 순서

새 Vendor Model로 Composition이 달라지므로 기존 Layer 7의 Provisioning 설정이 그대로 맞는다고 가정하지 않는다. 시험용 network에서 새 Composition을 확인하고 Vendor Model을 별도로 설정한다. 재Provisioning이나 NVS 삭제가 필요하면 해당 보드와 설정 손실을 설명하고 사용자 확인 후 진행한다. 자동 erase/factory-reset은 하지 않는다.

| 단계 | 확인 기준 |
| --- | --- |
| 호스트 | 8종 ID 왕복 codec, 0/invalid 처리, 잘못된 길이·version, 반복 동일 ID, self 제외, 방향별 라우팅, queue full, 만료, 미준비·API 실패 처리 |
| 빌드 | 새 ESP32-S3 프로젝트 빌드, 기존 STM32 Debug/Release 및 센서 기능 설정별 기준 빌드. 실제 실행 성공으로 표시하지 않음 |
| 로컬 UART | STM32 TX와 ESP32 RX의 같은 ID 확인. 반대 방향은 STM32의 RX/invalid/drop 카운터와 출력 확인 |
| 두 노드 종단 간 | 각 ID를 한 건씩 보내고 다음 입력 전 수신 확인. A→B와 B→A에서 ESP32 Mesh RX, STM32 remote count/last ID 확인 |
| 출력 | 버튼은 음성 요청, 후방은 상태 변화, 낙차·SOS는 기존 latch 정책에 맞게 확인. 각 사건이 매번 같은 출력을 반복해야 한다고 가정하지 않음 |
| 과부하 | 입력을 몰아 drop/만료가 드러나는지 확인. 저속 성공과 과부하 무손실 보장을 구분 |
| 세 노드 | 같은 원본 이벤트를 두 다른 STM32가 받는지 확인. 별도로 직접 경로 차단 조건의 Relay OFF/ON 대조 |

최초 종단 간 PASS는 로그만으로 선언하지 않는다. STM32의 실제 수신 카운터·ID와 출력 관찰을 함께 남긴다. 세 노드가 가까이서 모두 수신한 결과는 멀티홉 Relay 증거가 아니다.

## 8. 다음 단계

위 경로 검증 후 STM32 생산 데이터에 종류·단위·값·source·유효성·측정 버전을 붙인다. 이벤트와 주기 데이터를 구분하고, 송신 주기·만료·수신 상태 조회·우선순위를 추가한다. 현재 두 바이트 Mesh payload와 한 바이트 UART를 수치 전송 규격으로 그대로 사용하지 않는다.

이벤트 우선 통합을 선택한 이유는 STM32의 현재 동작을 유지한 채 통신 경로를 관찰할 수 있기 때문이다. 수치까지 한 번에 확장하는 대안은 UART/STM32/ESP32/Mesh 계약을 동시에 바꿔야 한다. 최종 목표는 둘 다 지원하는 것이며 수치 공유를 제외하기로 한 것은 아니다.
