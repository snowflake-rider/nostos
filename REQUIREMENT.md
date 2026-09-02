# NOSTOS Requirements

현재 세 노드는 [공통 장치](DEVICES.md)와 [배선](PINS.md)을 사용하고 센서 역할만 다릅니다. Protocol과
runtime 상태는 한 그룹에서 최대 10개 Rider Node를 지원해야 합니다. 장비 serial과 Mesh 키는 로컬에서만
관리합니다.

## 현재 노드 capability와 공유 데이터

| 현재 노드 profile | 센서 역할 | 그룹에 공유할 값 |
| --- | --- | --- |
| [Rider Node 1](RIDER-1.md) | XOSS 속도 센서 | 속도·휠 주행거리 |
| [Rider Node 2](RIDER-2.md) | DHT11 | 온도·습도 |
| [Rider Node 3](RIDER-3.md) | MPU6050 | 낙상 감지 시 `STOP_REQUEST(reason=FALL)` |

이 표는 현재 세 장비의 배치 profile일 뿐 고정 역할이 아닙니다. Rider Node ID는 위치나 센서 종류와
독립적이며, 이후 한 노드에 여러 capability를 조합하거나 같은 capability를 여러 노드에 배치할 수
있어야 합니다. 같은 capability를 여러 장비가 갖더라도 한 Topic의 active publisher는 한 노드만
둡니다. 라이더가 주행 중 앞뒤 순서를 바꿔도 Rider Node ID는 바뀌지 않으며 protocol은 현재 주행
위치를 저장하지 않습니다.

세 ESP32-S3는 같은 Bluetooth Mesh에 참여하고 직접 통신이 어려울 때 모두 relay할 수 있어야 합니다.
공유 응용 데이터는 `[속도, 휠 주행거리]`, `[온도, 습도]`, `[버튼 1/2]`,
`[STOP_REQUEST(reason=BUTTON|FALL)]`로 제한합니다. 센서와 요청 메시지는 공통 protocol로 전달하며
각 STM32의 dashboard에서 두 Shared State Feed의 최신값·발신 Node ID와 정지 요청의 발신 노드·원인을 확인해야 합니다. MPU6050
원시 가속도·자이로 값과 지속적인 낙상 Yes/No 상태는 로컬 판정에만 사용하고 Mesh로 공유하지 않습니다.

속도는 dashboard에서 5개의 원으로 표시합니다. 성인 자전거 443건의 자유 주행 관측값
`평균 20.62 km/h`, `표준편차 5.49 km/h`를 기준으로 0~42.6 km/h 범위를
`평균±표준편차` 구간으로 나눕니다. 0.1 km/h 단위 경계는 Level 1 `0.0~9.5`,
Level 2 `9.6~15.0`, Level 3 `15.1~26.0`, Level 4 `26.1~31.5`, Level 5 `31.6 이상`이며
유효하지 않은 값만 채운 원 없이 표시합니다. 42.6 km/h는 `평균+4σ`에 가까운 표시 범위의
참고 상단값이지 센서값 제한은 아닙니다. 기준값과 정상분포 적합성은
[FHWA의 13개 shared-use path 현장 연구](https://www.fhwa.dot.gov/publications/research/safety/pedbike/05137/chapter5.cfm)를
사용합니다.

## 버튼 broadcast

| 입력 | 메시지와 로컬 출력 |
| --- | --- |
| Button 1 | `PACE_REQUEST(action=ACCELERATE)` → 초록 LED → `pace_up.mp3` |
| Button 2 | `PACE_REQUEST(action=DECELERATE)` → 노랑 LED → `pace_down.mp3` |
| Button 3 | `STOP_REQUEST(reason=BUTTON)` → 빨강 LED → `stop.mp3` |
| Button 4 | 로컬 출력 reset 전용; Mesh message 없음 |

Rider Node 3의 로컬 낙상 판정이 정상에서 낙상으로 전이하면 `STOP_REQUEST(reason=FALL)`을 한 번 생성합니다.
낙상 상태가 계속되는 동안 요청을 반복 생성하지 않으며 별도의 FALL_CLEAR는 전송하지 않습니다.
Button 3 또는 낙상 감지로 생성된 Stop Request는 즉시 적용합니다.

## 전달 등급과 확장성

- application message 종류는 `STATE_UPDATE`, `PACE_REQUEST`, `STOP_REQUEST`, `STOP_ACK` 네 가지입니다.
- 모든 application message의 공통 header는 `type:u8 + source_node_id:u8` 두 바이트입니다.
- payload length와 CRC는 UART framing에서 처리하고 Bluetooth Mesh address는 수신 metadata로 처리합니다.
- 공용 STM32 binary는 STM32→paired ESP32 UART에서만 `source_node_id=0`을 사용합니다.
- ESP32는 local 요청의 source 0을 자신의 검증된 Rider Node ID로 교체한 뒤 Mesh로 보냅니다. Mesh와 ESP32→STM32 요청에서는 source 0을 거부하며, STM32→ESP32 local `STOP_ACK(source=0)`만 paired STM32의 application 수락 확인으로 허용합니다.
- Mesh의 `PACE_REQUEST`와 `STOP_REQUEST`는 임의의 non-zero 32-bit `request_id`를 사용합니다. STM32 local STOP은 boot-local non-zero counter를 쓰고 paired ESP32가 새 Mesh ID에 매핑합니다.
- `STOP_REQUEST`만 모든 의도된 peer의 수락을 확인하고 미수락 peer에 재시도합니다.
- peer는 Stop Request를 검증하고 paired STM32로 UART frame을 전달한 뒤 STM32의 local STOP_ACK를
  받아야 해당 `request_id`의 Mesh `STOP_ACK`를 발신자에게 보냅니다. 이 ACK는 STM32 application
  수락을 뜻하지만 OLED·audio 출력 완료를 뜻하지 않습니다.
- `PACE_REQUEST`와 센서 상태는 peer별 application ACK 없이 best-effort group message로 전달합니다.
- 별도 HEARTBEAT message를 추가하지 않고 각 active publisher가 값이 같아도 STATE_UPDATE를 주기적으로 반복합니다.
- application message에는 boot epoch, persistent session이나 application sequence를 넣지 않습니다.
- Bluetooth Mesh 자체의 sequence number와 replay protection은 Mesh stack에 맡깁니다.
- 센서 상태는 최신값으로 대체하며 중간 sample 유실을 허용합니다.
- 하나의 `STATE_UPDATE`에는 State Topic 하나와 그 topic의 최신 payload만 넣습니다.
- 여러 센서 Topic을 한 `STATE_UPDATE`에 묶지 않습니다.
- 현재 catalog는 속도·휠 주행거리를 묶은 `RIDE_STATE` 한 개와 온도·습도를 묶은 `ENVIRONMENT_STATE` 한 개입니다.
- 모든 Rider Node는 선택 설정 없이 `RIDE_STATE`와 `ENVIRONMENT_STATE`를 항상 수신·보관·표시합니다.
- 한 그룹에서 `RIDE_STATE` active publisher 하나와 `ENVIRONMENT_STATE` active publisher 하나만 둡니다.
- 다른 Rider Node는 받은 State Update를 application에서 다시 publish하지 않고 Bluetooth Mesh relay만 허용합니다.
- active publisher의 update가 끊겨도 다른 Rider Node를 자동 publisher로 선출하지 않습니다.
- 두 Shared State Feed는 모두 2초마다 STATE_UPDATE를 전송합니다.
- 마지막 수락한 update로부터 6초를 초과하면 STALE, 20초를 초과하면 UNKNOWN으로 바꿉니다.
- STALE은 마지막 값을 경고와 함께 표시하고 UNKNOWN은 값을 지웁니다.
- 두 State Topic payload는 공통으로 `sensor_valid:u8`를 가지며 `1`은 측정 가능, `0`은 측정 불가입니다.
- `sensor_valid=0`인 STATE_UPDATE도 publisher freshness를 갱신하지만 숫자 센서값은 표시하지 않습니다.
- `sensor_valid=0`일 때 해당 숫자 payload 필드는 모두 0으로 정규화합니다.
- 낙상은 State Topic으로 publish하지 않고 로컬 판정 후 `STOP_REQUEST(reason=FALL)`만 생성합니다.
- 센서 cache는 Topic ID별 최신값 하나와 해당 publisher의 Rider Node ID만 보관합니다.
- `RIDE_STATE`의 `wheel_distance`는 현재 boot에서 누적한 trip distance이며 재부팅하면 0으로 초기화합니다.
- 개인 심박수 같은 생체 데이터는 수집하거나 Mesh로 공유하지 않습니다.
- 새 센서 종류는 새 Topic ID와 `schema_rev=1` payload로 추가하며 공통 message envelope를 변경하지 않습니다.
- 기존 Topic ID의 payload 구조나 단위가 호환되지 않게 바뀔 때만 해당 topic의 `schema_rev`를 올립니다.
- 이전 firmware가 모르는 Topic ID나 Schema Revision은 안전하게 무시하고 기존에 아는 message 처리를 계속합니다.
- Bluetooth Mesh source address는 transport metadata로 받고 Mesh Address Binding을 통해 Rider Node ID로 해석합니다.

## UART framing

- STM32→ESP32와 ESP32→STM32는 `A5 5A | length:u8 | application_message | CRC16:u16LE` 한 형식을 사용합니다.
- `A5 5A`는 UART stream에서 frame 시작점을 찾는 표식이며 application message에 포함하지 않습니다.
- `length`는 Application Header와 type별 payload를 합친 application message의 바이트 수입니다.
- CRC는 `length + application_message`에 기존 CRC-16/CCITT-FALSE를 적용합니다.
- 별도 UART version, UART 전용 message type과 byte escaping을 사용하지 않습니다.
- 길이 또는 CRC가 잘못되면 frame을 버리고 다음 `A5 5A`를 탐색합니다.

## 재부팅 경계

- 재부팅하면 수신한 sensor cache를 비우고 dashboard를 `unknown` 상태로 시작합니다.
- 처리한 Pace Request·Stop Request 중복 기록, 받은 Stop Ack와 진행 중인 Stop retry를 복원하지 않습니다.
- UART parser buffer와 모든 application runtime queue를 비웁니다.
- 과거 application message를 flash에서 읽어 다시 실행하지 않습니다.
- Bluetooth Mesh key·provisioning 정보와 firmware의 Mesh Address Binding map만 유지합니다. Rider Node ID는 별도 application NVS 값으로 저장하지 않고, 재부팅 후 보존된 primary address와 map에서 다시 결정합니다.

## 구현·검증 경계

- 단일 네-message codec, 한 UART frame, Topic별 publisher/freshness 정책과 STOP의 peer-mask/ACK 정책은 host test로 검증합니다.
- ESP32 host test는 portable `application_runtime`까지만 포함하며 `bridge_runtime.c`, `mesh_node.c`, `xoss_ble.c`의
  ESP-IDF compile과 UART queue·Mesh send·BLE callback 통합은 검증하지 않습니다.
- Mesh relay는 firmware 기본값과 전송 ready 조건에 포함하지만 실제 거리·장애물에서의 relay 성공은 실물 검증이 필요합니다.
- 기본 STM32 개발 빌드는 DHT11·MPU6050을 끕니다. release-build는 Node 1 기본형,
  Node 2 DHT11, Node 3 MPU6050의 세 profile을 별도 artifact로 만들며 실물 센서 확인은 여전히 필요합니다.
- ESP32 target build에는 ESP-IDF v5.5.5 환경이 필요하며, 환경이 없는 host에서는 IDF compile을 성공으로 보고하지 않습니다.
- `ride/environment/STOP_REQUEST`의 `입력 → UART → Mesh → 상대 UART → STM32 local ACK → peer Mesh ACK → dashboard/audio` 실물 종단 간 검증은 별도로 수행합니다.
