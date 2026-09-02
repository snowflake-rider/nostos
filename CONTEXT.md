# Nostos

Nostos는 여러 자전거 노드가 라이더의 안전·협업 요청과 최신 주행·환경 상태를 공유하는 시스템이다.

## Language

**Confirmed Message**:
모든 의도된 peer가 수락했음을 확인해야 하는 Stop Request다. 반복 수신되더라도 같은 정지 요청을 반복 적용하지 않는다.
_Avoid_: Reliable Message, Confirmed Event

**Rider Request**:
라이더가 특정 시점에 다른 노드의 행동을 요청하는 일회성 메시지다. 늦게 도착한 요청은 현재 의도를 나타내지 않으므로 실행하지 않는다.
_Avoid_: Latest State, Permanent Command

**Pace Request**:
라이더가 그룹의 현재 속도를 올리거나 내리도록 요청하는 짧은 수명의 best-effort Rider Request다. 하나의 `PACE_REQUEST`가 `ACCELERATE` 또는 `DECELERATE` action을 가진다.
_Avoid_: Stop Request, Latest State

**Stop Request**:
라이더가 그룹에 지금 정지를 요청하는 안전 중심 Rider Request다. 물리 버튼이나 로컬 낙상 감지가 만들 수 있으며 즉시 적용한다.
_Avoid_: Pace Request, Fall State, Permanent Alarm

**Stop Reason**:
Stop Request가 물리 버튼 또는 로컬 낙상 감지 중 어디에서 시작됐는지 나타내는 원인이다. 낙상의 active/cleared lifecycle을 의미하지 않는다.
_Avoid_: Fall State, Incident Lifecycle

**Stop Ack**:
각 연결 구간의 application 수락을 같은 `request_id`로 확인하는 `STOP_ACK`다. peer ESP32는 paired STM32의 local STOP_ACK를 받은 뒤에만 발신자에게 Mesh STOP_ACK를 보낸다. OLED·audio 출력 완료나 사용자의 응답을 뜻하지 않는다.
_Avoid_: Output Completion, User Confirmation, Mesh Transport Ack

**Request ID**:
Pace Request와 Stop Request를 구분하는 non-zero 32-bit 값이다. Mesh ID는 ESP32가 임의로 만들고 STM32 local STOP ID는 boot-local counter를 사용한다. Stop Ack는 각 연결 구간에서 받은 Stop Request의 같은 Request ID를 돌려주며, 최근 ID 기록은 RAM에만 둔다.
_Avoid_: Boot Epoch, Persistent Sequence, Mesh Sequence Number

**Application Header**:
모든 Nostos application message 앞의 `type:u8 + source_node_id:u8` 두 필드다. boot/session/sequence, payload length, CRC와 Bluetooth address는 포함하지 않는다.
_Avoid_: UART Frame Header, Mesh Metadata, Message Payload

**Local Source Sentinel**:
공용 STM32 binary가 paired ESP32로 보내는 로컬 UART message에서만 사용하는 `source_node_id=0`이다. 요청은 ESP32가 검증된 자신의 Rider Node ID로 교체한다. STM32의 local STOP_ACK도 source 0을 사용하며, Mesh와 ESP32→STM32 요청에서는 금지한다.
_Avoid_: Rider Node ID, Unbound Mesh Source, Broadcast Source

**UART Start Marker**:
UART byte stream에서 새 frame의 시작점을 찾기 위한 고정 두 바이트 `A5 5A`다. application message의 일부가 아니며 값 자체에 업무 의미는 없다.
_Avoid_: Message Type, Protocol Version, Bluetooth Address

**UART Frame**:
STM32와 ESP32 사이에서 application message를 `A5 5A | length:u8 | application_message | CRC16:u16LE`로 감싼 한 가지 transport 형식이다.
_Avoid_: Application Message, Escaped Frame, Local Packet

**Latest State**:
가장 최근의 유효한 관측값만 의미가 있으며, 새 값으로 대체된 중간 관측값의 유실을 허용하는 센서 상태다.
_Avoid_: Telemetry Event, Confirmed Data

**State Update**:
한 Rider Node의 State Topic 하나와 그 최신 payload만 전달하는 `STATE_UPDATE`다. 여러 Topic을 한 message에 묶지 않으며 active publisher는 값이 변하지 않아도 주기적으로 다시 보낸다.
_Avoid_: State Bundle, Multi-topic Snapshot

**State Topic**:
같은 의미와 payload 구조를 공유하는 Latest State의 종류다. 현재 catalog는 속도·휠 주행거리를 묶은 `RIDE_STATE`와 온도·습도를 묶은 `ENVIRONMENT_STATE` 두 개이며 개인 생체 데이터는 포함하지 않는다.
_Avoid_: Rider Node Role, Arbitrary Key-Value Bag

**Topic ID**:
State Topic을 식별하는 protocol 상수다. 한 rider group에서는 각 Topic ID에 active publisher 하나만 둔다.
_Avoid_: Sensor Instance ID, Rider Node ID

**Schema Revision**:
한 Topic ID의 binary payload 구조와 단위에 붙는 작은 정수 revision이다. 첫 정의는 `1`이며 전체 protocol v1/v2나 firmware version과 무관하다.
_Avoid_: Protocol Version, Firmware Version

**Shared State Feed**:
모든 Rider Node가 선택 설정 없이 항상 수신·보관·표시하는 `RIDE_STATE`와 `ENVIRONMENT_STATE` 두 State Topic이다. 각 feed는 active publisher 하나의 최신값과 발신 Node ID를 가진다.
_Avoid_: Optional Subscription, Per-node Topic Selection

**Active Topic Publisher**:
한 rider group에서 특정 Shared State Feed를 실제 센서값으로 생성하는 유일한 Rider Node다. 다른 노드는 그 feed를 application에서 다시 publish하지 않고 수신·표시만 한다.
_Avoid_: Mesh Relay, Cached-value Re-broadcaster, Riding Position

**Feed Freshness**:
Shared State Feed의 마지막 수락한 State Update 이후 경과 시간에 따른 표시 상태다. 두 Topic 모두 2초마다 전송하고, 6초 초과면 `STALE`, 20초 초과면 `UNKNOWN`이다. STALE은 마지막 값을 경고와 함께 유지하고 UNKNOWN은 값을 표시하지 않는다.
_Avoid_: Sensor Validity, Automatic Publisher Failover, Separate Heartbeat

**Sensor Validity**:
State Update가 운반하는 `sensor_valid:u8` 값으로, publisher와 통신은 살아 있지만 실제 센서 측정값을 사용할 수 있는지 나타낸다. `1`은 valid, `0`은 invalid다.
_Avoid_: Feed Freshness, Publisher Liveness, Detailed Error Code

**Trip Distance**:
현재 boot의 휠 회전으로 누적한 주행거리다. 영구 odometer가 아니며 Rider Node가 재부팅하면 0에서 다시 시작한다.
_Avoid_: Lifetime Distance, Persistent Odometer

**Peer Acceptance**:
peer가 Confirmed Message를 검증하고 paired STM32 application의 local STOP_ACK까지 받은 상태다. Mesh Stop Ack로 발신자에게 알리며, 이 상태는 해당 peer에 대한 전달 재시도를 끝내지만 OLED·audio 출력 완료를 의미하지 않는다.
_Avoid_: Delivery Complete, Output Success

**Output Completion**:
peer의 물리 출력 담당 장치가 Confirmed Message에 따른 표시·소리 동작의 처리 결과를 보고한 상태다. 실패나 결과 유실은 원래 요청의 자동 재실행을 의미하지 않는다.
_Avoid_: Peer Acceptance, Automatic Retry

**Partial Delivery**:
Confirmed Message를 의도된 peer 중 일부만 수락한 상태다. 이미 수락한 peer의 결과는 유지하며, 아직 수락하지 않은 peer만 전달 재시도의 대상이 된다.
_Avoid_: Total Failure, Rollback

**Rider Node**:
한 rider group에 참여하는 물리 장비다. 현재 배치는 3개지만 한 그룹은 최대 10개 Rider Node를 갖는다.
_Avoid_: Sensor Role, Mesh Address

**Rider Node ID**:
한 rider group 안에서 물리 장비를 식별하는 `1..10` 번호다. `0`은 미할당을 뜻한다. 라이더 개인 번호나 주행 위치가 아니며, 라이더들이 앞뒤 순서를 바꿔도 바뀌지 않는다.
_Avoid_: Mesh Address, MAC Address, Sensor Role, Riding Position

**Riding Position**:
주행 중 계속 바뀔 수 있는 물리적인 앞뒤 순서다. 현재 Nostos protocol은 이를 정체성이나 routing 정보로 저장하지 않는다.
_Avoid_: Rider Node ID, Provisioned Role

**Capability**:
Rider Node가 제공하는 센서 입력, 로컬 출력 또는 relay 능력이다. 한 노드는 여러 Capability를 가질 수 있고 Capability가 Rider Node ID를 결정하지 않는다.
_Avoid_: Node Identity, Fixed Role

**Mesh Address Binding**:
Rider Node ID와 현재 Bluetooth Mesh primary unicast address 사이의 검증된 연결이다. 현재 firmware에서는 `CONFIG_NOSTOS_SOURCE1..10_ADDRESS` map으로 표현한다. 재-provision으로 address가 바뀌면 이 map도 갱신하지만 Rider Node ID는 바뀌지 않는다.
_Avoid_: Rider Node ID, Hard-coded Address

**Mesh Source Address**:
Bluetooth Mesh가 수신 packet과 함께 제공하는 현재 발신 unicast address다. 수신자는 Mesh Address Binding으로 이를 Rider Node ID에 해석하며 dashboard의 영구 정체성으로 사용하지 않는다.
_Avoid_: Rider Node ID, BLE MAC Address

**Provisioning Record**:
물리 장비에 Rider Node ID를 할당하고 현재 Mesh Address Binding을 연결하는 권한 있는 배치 기록이다. Mesh stack은 provisioning 정보를 NVS에 보존하고, application은 현재 primary address를 firmware address map에 대입해 runtime ID를 결정한다.
_Avoid_: Runtime Guess, Mesh Address

**Unbound Node**:
유효하고 고유한 Rider Node ID와 Mesh Address Binding을 증명하지 못한 노드다. Bluetooth Mesh에 provisioned됐을 수 있지만 Nostos message에는 참여하지 않는다.
_Avoid_: Unprovisioned Mesh Node, Unknown Peer

**Runtime State**:
현재 boot에서만 유효한 sensor cache, 최근 Request ID, Stop Ack, retry 상태와 parser buffer다. 재부팅하면 복원하거나 재실행하지 않고 전부 비운다.
_Avoid_: Provisioning Record, Persistent Configuration

**Persistent Configuration**:
재부팅 후에도 유지하는 Bluetooth Mesh key·provisioning 정보와 firmware의 Mesh Address Binding map이다. Rider Node ID는 별도 NVS 값이 아니라 이 둘에서 다시 결정한다. sensor 선택 설정과 application message history는 포함하지 않는다.
_Avoid_: Runtime State, Message Archive
