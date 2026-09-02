# NOSTOS 공통 application protocol

STM32와 ESP32가 함께 사용하는 단일 wire 계약입니다. 이 디렉터리의 production code는 공통 codec
`nostos_protocol.*`과 UART framing `nostos_uart.*` 두 모듈뿐입니다. 화면·오디오용 STM32 로컬 enum은
wire protocol과 섞지 않고 `firmware/stm32/MyApp/common/message_type.h`에 둡니다.

## 메시지 네 종류

모든 application message는 다음 2바이트 header로 시작합니다.

```text
type:u8 | source_node_id:u8 | payload
```

| type | 이름 | payload | 전체 길이 |
| ---: | --- | --- | ---: |
| `01` | `STATE_UPDATE` | Topic에 따라 아래 참조 | 9B 또는 11B |
| `02` | `PACE_REQUEST` | `request_id:u32LE, action:u8` | 7B |
| `03` | `STOP_REQUEST` | `request_id:u32LE, reason:u8` | 7B |
| `04` | `STOP_ACK` | `request_id:u32LE` | 6B |

정상 application과 Mesh에서 `source_node_id`는 `1..10`입니다. 장비 번호이며 라이더의 이름,
주행 순서, 센서 종류가 아닙니다. Bluetooth Mesh 주소는 application payload에 복사하지 않습니다.
수신 ESP32 runtime이 Mesh source metadata를 provisioned primary address와 firmware의 Node ID address map에 비교합니다.

공용 STM32 firmware에는 Node ID 설정이 없으므로 **STM32→paired ESP32 로컬 UART에서만** source
`0`을 허용합니다. 요청에서 source `0`을 받은 ESP32는 자신의 provisioned Node ID로 바꾼 뒤 정상
codec으로 다시 검증합니다. 수신 STM32가 보내는 local `STOP_ACK(source=0)`은 그 STM32의 application
수락을 뜻합니다. source `0`은 Mesh로 절대 전송하지 않습니다. ESP32→STM32 요청에는 실제
`1..10`을 사용합니다.

## State Update

한 `STATE_UPDATE`에는 Topic 하나만 있습니다.

```text
topic_id:u8 | schema_rev:u8=1 | sensor_valid:u8 | topic_data
```

| topic | topic_id | topic_data |
| --- | ---: | --- |
| `RIDE_STATE` | `01` | `speed_x10_kmh:u16LE, trip_distance_m:u32LE` |
| `ENVIRONMENT_STATE` | `02` | `temperature_x10_c:i16LE, humidity_x10_pct:u16LE` |

예를 들어 속도 `205`는 `20.5 km/h`, 온도 `-55`는 `-5.5°C`입니다. `trip_distance_m`는
현재 boot의 주행거리이며 재부팅하면 0이 됩니다. `sensor_valid=0`이면 모든 topic 숫자 필드는
wire에서 0이어야 합니다. encoder는 invalid 상태의 숫자를 0으로 정규화하고 decoder는 0이 아닌
invalid wire를 거부합니다.

그룹에서는 Topic마다 active publisher 하나만 운영하고 모든 Node가 두 Topic을 수신합니다. publisher는
값이 같아도 2초마다 `STATE_UPDATE`를 반복합니다. freshness와 publisher 설정은 codec이 아닌 runtime
정책입니다: 마지막 수신 후 6초 초과는 `STALE`, 20초 초과는 `UNKNOWN`입니다. 수신 Node는 받은
state를 application에서 재-broadcast하지 않습니다.

## Request와 STOP

| enum | 값 |
| --- | ---: |
| `NOSTOS_PACE_ACCELERATE` | `01` |
| `NOSTOS_PACE_DECELERATE` | `02` |
| `NOSTOS_STOP_REASON_BUTTON` | `01` |
| `NOSTOS_STOP_REASON_FALL` | `02` |

Button 1/2는 즉시 `PACE_REQUEST`, Button 3은 즉시 `STOP_REQUEST(reason=BUTTON)`을 만듭니다.
낙상 원시값은 Mesh로 보내지 않고 로컬 판정이 `STOP_REQUEST(reason=FALL)`을 만듭니다.

`PACE_REQUEST`는 paired ESP32가 Mesh로 내보내기 전에 non-zero 32비트 난수 ID를 붙입니다.
`STOP_REQUEST`는 UART와 Mesh에서 ID의 역할이 다릅니다.

```text
STM32 local STOP id --paired UART--> ESP32 --new random Mesh id--> peers
          ^                         |
          +------ local STOP_ACK ---+  (ESP32 runtime 수락 확인)
```

STM32는 boot-local non-zero counter를 사용하고 같은 STOP을 paired ESP32가 수락할 때까지 200ms마다
같은 local ID로 재전송합니다. ESP32는 새 local STOP을 RAM retry 상태에 먼저 등록한 뒤 같은 local ID로
즉시 `STOP_ACK`를 보냅니다. 이 ACK는 peer 수신이나 audio/OLED 완료 확인이 아니라 **paired ESP32가
책임을 넘겨받았다는 확인**입니다. ESP32는 local ID 하나를 non-zero random Mesh ID 하나에 매핑하고,
peer의 Mesh `STOP_ACK`는 이 Mesh ID를 echo합니다.

반대편 peer는 Mesh STOP을 검증한 뒤 paired STM32로 전달하고, STM32가 같은 ID의 local
`STOP_ACK(source=0)`을 반환할 때까지 Mesh ACK를 보내지 않습니다.

```text
sender ESP --Mesh STOP--> peer ESP --UART STOP--> peer STM32
sender ESP <--Mesh ACK--- peer ESP <--local ACK-- peer STM32
```

따라서 Mesh `STOP_ACK`는 상대 ESP의 UART 송신 완료가 아니라 상대 STM32 application까지 요청이
도달해 수락되었다는 뜻입니다. 다만 audio/OLED 같은 물리 출력의 완료나 성공을 뜻하지는 않습니다.
pending duplicate는 STM32로 다시 전달하고, STM32가 이미 수락한 duplicate에는 출력 동작을 반복하지
않고 local ACK만 다시 보냅니다.

`FALL`은 `BUTTON`보다 우선합니다. 대기 중인 BUTTON이 FALL로 바뀌면 새 local ID와 새 Mesh ID를
사용하고 전체 peer pending mask를 다시 채웁니다. 이미 대기 중인 FALL은 뒤의 BUTTON으로 낮추지
않습니다. 이전 ID의 늦은 ACK는 현재 STOP을 완료하지 못합니다. PACE와 STATE에는 ACK가 없습니다.

Application boot epoch, session, sequence, heartbeat는 없습니다. 최근 request ID, STOP retry/ACK,
parser와 state cache는 RAM에만 두고 재부팅 시 비웁니다. ESP32는 local ACK 유실에 답하기 위해 마지막
local STOP 매핑을 2초 동안만 보존하고, 그 안의 동일 ID/reason 재수신에는 새 Mesh STOP 없이 ACK만
다시 보냅니다. 이 창이 지난 뒤 독립 재부팅으로 같은 local ID가 재사용되면 새 STOP으로 처리합니다.
영구 epoch를 추가해 복잡도를 높이는 대신, 매우 드문 중복 STOP이 새 STOP을 조용히 억제하는 것보다
안전하다는 정책입니다. Bluetooth Mesh 자체 sequence와 replay protection은 Mesh stack의 책임입니다.

ESP32 ingress는 `STOP_REQUEST` 10칸, `STOP_ACK` 10칸, 일반 traffic 16칸의 정적 queue를 분리합니다.
두 sensor Topic은 FIFO에 쌓지 않고 Topic별 latest slot 하나씩만 두어 새 값이 아직 처리되지 않은 옛 값을
대체합니다. worker는 `STOP_REQUEST > STOP_ACK > latest sensor state > normal` 순서로 처리합니다. control
queue가 가득 차면 frame을 버리되 성공 ACK를 만들지 않으므로 기존 sender retry가 회복을 담당합니다.

Mesh model 설정이 일시적으로 ready가 아니면 새 ingress는 ACK하지 않고 버리지만, 이미 paired STM32에
ACK한 STOP의 pending mask와 volatile state는 같은 primary address가 다시 ready가 될 때까지 유지합니다.
primary address가 사라지거나 다른 Node identity로 바뀌면 이 RAM 상태를 비웁니다. 상태 로그의
`mesh_tx_api_accepted`와 `mesh_tx_complete_ok`는 각각 ESP-IDF API 수락과 stack send-complete일 뿐이며,
peer 수신 증거는 반드시 해당 request ID의 Mesh `STOP_ACK`로 확인합니다.

## UART

STM32↔ESP32 양방향 framing은 하나입니다.

```text
A5 5A | length:u8 | application_message | CRC16:u16LE
```

- `length`: header를 포함한 application message 길이 `6..11`
- CRC 범위: `length + application_message`
- CRC: CRC-16/CCITT-FALSE, poly `1021`, init `FFFF`
- timeout: frame 도중 마지막 byte 이후 100ms 초과
- escaping, 별도 UART version, UART 전용 type 없음
- length를 읽은 뒤에는 payload의 `A5 5A`를 새 시작값으로 보지 않음
- 길이/CRC/timeout 오류 뒤 다음 `A5 5A`에서 재동기화

일반/Mesh 경계는 `nostos_message_encode/decode`, UART source `0` 예외는 명시적인
`nostos_local_message_encode/decode`와 `nostos_uart_*_local_message` API만 사용합니다.

## 검사

```sh
bash firmware/tools/fw check protocol
bash firmware/tools/fw test protocol
```

Host tests는 네 메시지 golden bytes와 round trip, 잘못된 type/topic/source/revision/length/enum,
invalid sensor 정규화, source `0`의 local STOP_ACK 경계, CRC, fragmentation, timeout과
resynchronization을 검사합니다.
Target build는 compile 증거이며 실제 UART/Mesh/출력 성공은 별도 실물 검증이 필요합니다.
