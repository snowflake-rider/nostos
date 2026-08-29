> 이관 원문: `docs/01-project/STATUS.md`. 현재 실행 경로는 [팀원 시작 안내](../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 진행 상태와 검증 기록

[전체 시작 메뉴](../records/esp-ble-original-index.md) · [프로젝트 개요](OVERVIEW.md) · [학습 로드맵](../learning/LAYER-ROADMAP.md) · [통신 API](../../../experiments/communication-module/README.md)

최신 정리 기준일: **2026-08-28**. 8월 28일에 추가된 Layer 7/8, STM32 버튼, iPhone GPS Mesh 기록을 현재 문서 기준으로 모았다. **이번 문서 갱신 중 보드 연결 상태를 재확인하거나 빌드·Flash·무선 시험을 다시 실행한 것은 아니다.** 포트와 보드 배치는 각 기록 당시의 값이다.

이 문서는 현재까지 기록된 완료 범위와 실행 증거를 관리한다. 학습 순서·통과 기준은 로드맵에, API 계약은 코드 옆 README에 둔다.

## 최신 진행 요약 — 2026-08-28

| 경로 | 현재 확인된 범위 | 아직 확인하지 않은 범위 |
| --- | --- | --- |
| Layer 0–5 | 2026-08-27 기록 기준 단계별 Source/Build/Flash/Runtime과 해당 직접 통신 PASS | 이번 문서 갱신에서 재실행하지 않음 |
| [Layer 6](../layers/layer-6/README.md) custom forwarding | 두 보드 direct RX와 forward TX PASS | 세 고유 보드의 exact `origin -> forward -> relayed RX` chain |
| [Layer 7](../layers/layer-7/README.md) 표준 Mesh | 세 노드 Flash/boot, iPhone Provisioning/configuration, group `0xC000` OnOff, acknowledged Status, TX power read-back PASS | 직접 경로 차단 조건의 controlled Relay OFF/ON 비교, `0x0003`/`0x0004`를 sender로 사용한 반대 방향 serial test |
| [Layer 8](../../../firmware/esp32/docs/layer8-background.md) UART ↔ Mesh | Host Debug/Release/ASan+UBSan과 ESP32-S3 build PASS, `0x0003`/`0x0004` Flash·boot·USB status PASS, `0x0005` Flash PASS, 후속 관찰에서 세 노드 `event_ready=1` | STM32 UART가 ESP32에 실제 도착하는지, C001 Mesh 전송·상대 UART 출력, 부하·Relay 검증 |
| [STM32 버튼 시험](../../verification/stm32-button-uart.md) | PB6 입력, USART2 송신, NUCLEO Flash, ST-LINK VCP의 `0x13` 수신 확인 | NUCLEO 외부 D1/PA2 → ESP32 GPIO18 경로와 이후 Mesh 송수신 |
| [Communication Module](../../../experiments/communication-module/README.md) | 8칸 Event FIFO, 5개 속도 평균, Periodic/Service API와 호스트 검사 | 이 모듈 자체의 UART wire, BLE/Mesh transport, 수신 API·SharedState·화면 통합 |
| [iPhone GPS Mesh](../../../apps/ios-gps-mesh/README.md) | Swift/C 검사, ESP32 GPS node build, unsigned iPhone 앱 build, Simulator 실행·위치 권한 흐름 PASS | 실제 iPhone 서명·설치, GPS/Bluetooth Proxy/Mesh 송신, ESP32 Flash, 잠금·재연결·세 노드 수신 |

Layer 7의 현재 안정 식별자는 `0x0003-ESP32-76`, `0x0004-ESP32-B6`, `0x0005-ESP32-D6`다. 임시 A/B/C 명칭만으로 보드 상태를 기록하지 않는다.

### 현재 막힌 지점

STM32 외부 PB6 버튼을 누르면 USART2 송신과 ST-LINK VCP 수신까지는 확인된다. 그러나 같은 관찰 구간에 ESP32 세 대의 UART valid 수신 및 Mesh TX/RX 카운터는 증가하지 않았다. 따라서 현재 다음 실물 확인 지점은 **NUCLEO D1/PA2 외부 경로와 Solder Bridge 상태, 공통 GND, ESP32 GPIO18 배선**이다. 이 구간을 확인하기 전에는 Layer 8 UART 또는 Mesh 성공으로 표시하지 않는다.

근거:

- [Layer 8 두 보드 설치·초기 상태](../../../firmware/esp32/docs/VERIFICATION.md)
- [B6 AppKey/Publication 진단과 설정 절차](../../../firmware/esp32/docs/B6_SETUP.md)
- [STM32 버튼·USART2 실제 관찰 결과](../../verification/stm32-button-uart.md)
- [현재 배선용 빠른 버튼 → Mesh 관찰 도구](../../../firmware/esp32/docs/FAST_CHECK.md)
- [iPhone GPS Mesh 구현 검증 기록](../designs/plans/2026-08-28-iphone-gps-mesh-verification.md)

### 통신 코드 경로 구분

- `communication-module/`: 버튼·속도 송신 정책을 검증하는 공통 C API다. 아직 실제 wire/보드 transport와 결합하지 않았다.
- `stm32-project/integration/esp32-s3/`: 팀 통합을 위한 STM32 Event ↔ ESP32 Mesh bridge 원본 경로다.
- `layers/layer-8/`: 위 Event bridge를 독립적으로 학습·Flash하기 위한 복사본이다. 원본과 자동 동기화되지 않는다.

한 경로의 Host PASS나 Build PASS를 다른 경로의 실제 UART/Mesh PASS로 승계하지 않는다. 계약을 변경할 때는 원본과 Layer 8 복사본을 함께 확인한다.

## 기본 Communication Module — 기록된 구현 범위

| 영역 | 상태와 근거 범위 |
| --- | --- |
| 공통 메시지·Event FIFO | 버튼 코드 1/2/3, 8칸 큐, 호스트 검사 구현 |
| Periodic | 최근 5개 속도 평균, 유효성·만료·주기·BUSY 처리 구현 |
| Service API | 이벤트 우선 선택, burst 한도, due 속도 전송 기회, 단일 실행 흐름용 송신 API 구현 |
| 호스트 검증 | 기존 기록: CTest 7개 항목, 동작 검사 41개와 예제 3개. 이번 문서 정리에서는 재실행하지 않음 |
| 실제 통합 | 센서 어댑터, UART wire 처리, 이 모듈의 실제 BLE/Mesh 송수신, 수신 API·SharedState, RTOS Task 구성은 미구현 |

구체적인 구현·시험 구성은 [모듈 README](../../../experiments/communication-module/README.md), 호출 제한은 [Service API](../../../experiments/communication-module/service/README.md)를 따른다. `comm_process()`의 송신 콜백 접수 성공은 상대 수신 ACK가 아니다.

## 2026-08-27 당시 Layer 검증 요약 — 보존 기록

| Layer | 목표 | Source | Build | Flash | Runtime | Radio |
|---|---|---:|---:|---:|---:|---:|
| [Layer 0](../layers/layer-0/README.md) | ESP32-S3 확인부터 `app_main()` 실행까지 | 완료 | PASS | PASS | PASS | 해당 없음 |
| [Layer 1](../layers/layer-1/README.md) | non-connectable BLE Advertising 시작 | 완료 | PASS | PASS | PASS | phone 별도 확인 기록; 자동 로그는 미검증 |
| [Layer 2](../layers/layer-2/README.md) | GATT Read/Write, ACK, Notification | 완료 | PASS | PASS | PASS | phone 검증 PASS |
| [Layer 3](../layers/layer-3/README.md) | Layer 2 이름 + Service UUID Active Scan | 완료 | PASS | PASS | PASS | peer 수신 PASS |
| [Layer 4](../layers/layer-4/README.md) | GATT Server + Advertising + Active Scan | 완료 | PASS | PASS (2대) | PASS (2대) | 양방향 peer PASS |
| [Layer 5](../layers/layer-5/README.md) | 20-byte packet + CRC + sequence + dedup | 완료 | PASS | PASS (2대) | PASS (2대) | 양방향 packet PASS |
| [Layer 6](../layers/layer-6/README.md) | symmetric forwarding + TTL + path 구분 | 진행 중 | PASS | PASS (2대) | PASS (2대) | pair forward PASS, triplet 미검증 |
| [Layer 7](../layers/layer-7/README.md) | 표준 Mesh symmetric OnOff + iPhone Provisioning + Relay | 진행 중 | PASS | PASS (2대) | PASS (2대) | unprovisioned boot PASS, Mesh radio 미검증 |

아래 표는 2026-08-27 당시 상태를 보존한 것이다. Layer 1은 자동 실행 로그의 `OVER_AIR_SCAN=NOT_VERIFIED`와 별도 phone 확인 기록을 구분했다. Layer 6/7의 당시 “진행 중”은 일부 단계의 PASS를 전체 완료로 확장하지 않기 위한 표시다. 최신 상태는 문서 위쪽의 2026-08-28 요약을 따른다.

## 2026-08-27 당시 남은 작업과 검증 — 보존 기록

### 무선 학습 경로

- Layer 6: 세 번째 고유 보드를 포함한 exact custom relay chain 실물 검증.
- Layer 7: iPhone에서 세 Node PB-GATT Provisioning.
- AppKey와 Generic OnOff Server/Client Model Bind.
- group `0xC000` Publication/Subscription과 세 Node Generic OnOff 수신.
- 직접 경로 차단 조건에서 Relay OFF/ON 대조 검증.

### 팀 통신 모듈 경로

- 데이터·wire 규약, 수신 API·SharedState, 유효성·만료·중복 처리.
- RTOS 입력 전달과 상태 소유권, 부하 상황의 Event/Periodic 처리 검증.
- Head BLE 센서와 STM32 UART 어댑터 통합.
- 검증된 Mesh에 모듈을 연결하고 실제 사용자 데이터·화면·Relay 확인.

구현 순서의 후보와 미결정 사항은 [프로젝트 개요](OVERVIEW.md#open-decisions)에서 관리한다. MPU6050 부착 위치는 몸/자전거 비교 실험으로 고르며 통신 인터페이스 설계의 선행 차단 조건으로 두지 않는다.

## 2026-08-27 실행 증거

아래 기록은 기존 문서에서 이관했다. 두 문서의 일부 로그 발췌가 겹치더라도 당시 관찰과 설명을 보존한다. `NOT_VERIFIED`를 실제 시험 없이 PASS로 바꾸지 않는다.

<details>
<summary>보드 배치와 단계별 실행 로그 — 기존 진행 기록</summary>

### 2026-08-27 physical setup

1. Board A: Layer 7 standard Mesh firmware, `/dev/cu.usbmodem1101`, node `76`
2. Board B: Layer 7 standard Mesh firmware, `/dev/cu.usbmodem1401`, node `B6`
3. Board C: not connected; Layer 7 triplet verification pending

### Verified checkpoints

- Layer 0: build, flash, and `app_main()` runtime PASS
- Layer 1: BLE Advertising runtime PASS; phone scan confirmed separately
- Layer 2: phone connection, GATT Read/Write, ACK, Notification toggle,
  disconnect, and Advertising restart confirmed
- Layer 3: build and flash PASS; Board B received Board A over the air by
  matching both `ESP32-LAYER-2` and Service UUID
  `7A110000-6B0D-4D5A-8F4B-2C9E00000001`
- Layer 4: source, clean build, two-board flash, local GATT + Advertising +
  Active Scanning, Mac CoreBluetooth reception, and bidirectional `PEER_RX` PASS
- Layer 5: host packet tests, source, clean build, two-board flash, bilateral
  `PACKET_TX`, and bidirectional `PACKET_RX_NEW ... crc=ok` PASS
- Layer 6: packet/relay host tests, ASan/UBSan, ESP-IDF build, two-board flash,
  bilateral `PACKET_RX_DIRECT`, and bilateral `PACKET_TX_FORWARD ttl=1` PASS;
  three-board relayed RX remains not verified
- Layer 7: serial parser host test + ASan/UBSan, ESP-IDF build, two-board flash,
  distinct identity, PB-GATT/PB-ADV, GATT Proxy, and unprovisioned runtime PASS;
  iPhone Provisioning/Configuration, group OnOff, and Relay remain not verified

Layer 3 physical result:

```text
[LAYER-3] TARGET_FOUND name=ESP32-LAYER-2 rssi=-39
[LAYER-3] SERVICE_MATCH uuid=7A110000-6B0D-4D5A-8F4B-2C9E00000001
[LAYER-3] TARGET_RX count=1 rssi=-39
RESULT=PASS
PEER_OVER_AIR_SCAN=PASS
```

Evidence: [esp32s3-layer-3-active-scanner-20260827T142229-KST.log](../layers/layer-3/logs/esp32s3-layer-3-active-scanner-20260827T142229-KST.log)

Layer 4 staged one-board result (before the pair test):

```text
[LAYER-4] NODE_READY id=B6 name=ESP32-L4-B6
[LAYER-4] ADVERTISING_STARTED
[LAYER-4] SCANNING_STARTED mode=active
[LAYER-4] DUAL_ROLE_ACTIVE ... gatt=yes advertising=yes scanning=yes
LOCAL_GATT_ADV_SCAN=PASS
PAIR_PEER_RX=NOT_VERIFIED
```

Evidence: [esp32s3-layer-4-dual-role-20260827T144822-KST.log](../layers/layer-4/logs/esp32s3-layer-4-dual-role-20260827T144822-KST.log)

Layer 4 staged external Advertising evidence (before the pair test):

```text
LAYER4_OVER_AIR name=ESP32-L4-B6 rssi=-38 service_match=true
LAYER4_OVER_AIR_ADV=PASS
ONE_BOARD_OVER_AIR_ADV=PASS
PAIR_PEER_RX=NOT_VERIFIED
```

Evidence: [esp32s3-layer-4-over-air-adv-20260827T145200-KST.log](../layers/layer-4/logs/esp32s3-layer-4-over-air-adv-20260827T145200-KST.log)

The two staged records above intentionally retain `PAIR_PEER_RX=NOT_VERIFIED`.
The following later pair workflow is the final Layer 4 result.

Layer 4 final two-board result:

```text
[A] PEER_RX local=76 peer=B6 count=1 rssi=-31
[B] PEER_RX local=B6 peer=76 count=1 rssi=-31
BOARD_A_NODE=76
BOARD_B_NODE=B6
BIDIRECTIONAL_PEER_RX=yes
RESULT=PASS
PAIR_ADV_SCAN=PASS
PAIR_GATT_SERVER=PASS
PAIR_PEER_RX=PASS
```

Evidence: [esp32s3-layer-4-dual-role-pair-20260827T145221-KST.log](../layers/layer-4/logs/esp32s3-layer-4-dual-role-pair-20260827T145221-KST.log)

Layer 5 final two-board result:

```text
[A] PACKET_TX sender=76 seq=2 ttl=2 recipient=FF len=5 payload=HELLO crc=ok
[B] PACKET_TX sender=B6 seq=2 ttl=2 recipient=FF len=5 payload=HELLO crc=ok
[A] PACKET_RX_NEW local=76 sender=B6 seq=2 ... payload=HELLO crc=ok rssi=-30
[B] PACKET_RX_NEW local=B6 sender=76 seq=2 ... payload=HELLO crc=ok rssi=-32
BOARD_A_NODE=76
BOARD_B_NODE=B6
BIDIRECTIONAL_PACKET_RX_NEW=yes
RESULT=PASS
PAIR_ADV_SCAN=PASS
PAIR_GATT_SERVER=PASS
PAIR_PACKET_TX=PASS
PAIR_PACKET_RX=PASS
PAIR_PACKET_CRC=PASS
```

Evidence: [esp32s3-layer-5-packet-node-pair-20260827T150742-KST.log](../layers/layer-5/logs/esp32s3-layer-5-packet-node-pair-20260827T150742-KST.log)

Layer 5 is direct packet reception only. Forwarding, three-board relay, and
Bluetooth Mesh remain not implemented and not verified.

Layer 6 current two-board result:

```text
[A] PACKET_TX_ORIGIN local=76 origin=76 seq=1 ttl=2 ... crc=ok
[B] PACKET_RX_DIRECT local=B6 origin=76 via=76 seq=1 ttl=2 ... duplicate=no crc=ok
[B] PACKET_TX_FORWARD local=B6 origin=76 seq=1 ttl=1 ... crc=ok
[B] PACKET_TX_ORIGIN local=B6 origin=B6 seq=1 ttl=2 ... crc=ok
[A] PACKET_RX_DIRECT local=76 origin=B6 via=B6 seq=1 ttl=2 ... duplicate=no crc=ok
[A] PACKET_TX_FORWARD local=76 origin=B6 seq=1 ttl=1 ... crc=ok
BOARD_A_NODE=76
BOARD_B_NODE=B6
BIDIRECTIONAL_DIRECT_RX=yes
BIDIRECTIONAL_FORWARD_TX=yes
RESULT=PASS
PAIR_FORWARD_TX=PASS
TRIPLET_RELAY=NOT_VERIFIED
```

Evidence: [esp32s3-layer-6-relay-node-pair-20260827T152751-KST.log](../layers/layer-6/logs/esp32s3-layer-6-relay-node-pair-20260827T152751-KST.log)

Layer 6 forwarding is implemented and physically verified on the two connected
boards. A third distinct node has not yet produced the required
`origin A -> direct RX B -> forward B -> relayed RX C` chain. This custom BLE
forwarding is also not standard Bluetooth Mesh Relay.

Layer 7 current two-board boot result:

```text
[A] NODE_IDENTITY node=76 name=ESP32-MESH-76 uuid=7A11070000000000000014C19FCEEC76
[A] UNPROVISIONED_READY bearers=PB-GATT|PB-ADV err=0
[A] STATUS ... provisioned=no ... relay=disabled
[B] NODE_IDENTITY node=B6 name=ESP32-MESH-B6 uuid=7A110700000000000000441BF6FFBAB6
[B] UNPROVISIONED_READY bearers=PB-GATT|PB-ADV err=0
[B] STATUS ... provisioned=no ... relay=disabled
```

Evidence:

- [esp32s3-layer-7-standard-mesh-20260827T160646-KST.log](../layers/layer-7/logs/esp32s3-layer-7-standard-mesh-20260827T160646-KST.log)
- [esp32s3-layer-7-standard-mesh-20260827T160701-KST.log](../layers/layer-7/logs/esp32s3-layer-7-standard-mesh-20260827T160701-KST.log)

두 보드는 같은 Layer 7 image로 boot했지만 아직 iPhone network에
Provisioning되지 않았다. 따라서 AppKey, Model Bind, group `0xC000`, Generic
OnOff 수신, 세 보드 standard Mesh Relay는 모두 `NOT_VERIFIED`다.

</details>

<details>
<summary>Layer별 결과 해석과 증거 경계 — 기존 로드맵에서 이관</summary>

Layer 1의 스마트폰 scan과 Layer 2의 GATT 기능은 별도 실물 테스트로 확인했다.
Layer 3는 Board B가 독립 전원으로 동작하는 Board A의 Advertising과 scan
response를 실제 수신해 `TARGET_RX`까지 PASS했다.
Layer 4는 동일 firmware를 두 보드에 Flash하고, 양쪽에서 GATT Server,
Advertising, Active Scanning과 상호 `PEER_RX`를 확인했다.
Layer 5는 같은 두 보드가 `HELLO` custom packet을 송신하고, 상대 packet을
CRC 검증 후 `PACKET_RX_NEW`로 처리하는 것을 양방향으로 확인했다.
Layer 6는 같은 두 보드가 상대 origin packet을 `PACKET_RX_DIRECT`로 받고,
origin/sequence를 유지하며 TTL을 2에서 1로 감소시켜
`PACKET_TX_FORWARD`하는 것을 양방향으로 확인했다. 세 번째 고유 node의
`PACKET_RX_RELAYED` chain은 아직 확인하지 않았다.
Layer 7은 같은 standard Mesh image를 두 보드에 Flash해 서로 다른 identity,
PB-GATT/PB-ADV, GATT Proxy, Relay-disabled boot까지 확인했다. iPhone에서
Provisioning/Configuration하고 세 보드 group message와 Relay를 확인하는
단계는 아직 수행하지 않았다.

### 현재 Layer 0 증거

Layer 0의 자동 workflow는 다음 marker를 기다린다.

```text
[LAYER-0] BOOT_SUCCESS target=esp32s3 idf=v5.5.5
[LAYER-0] RUNTIME_OK count=...
RESULT=PASS
STAGE=complete
```

2026-08-27 현재 실제 build, Flash, Runtime PASS 기록:

- [Layer 0 실행 로그](../layers/layer-0/logs/esp32s3-layer-0-bootload-20260827T122023-KST.log)

이 기록은 그 실행 당시의 보드 결과다. 다음 실행에서도 같은 결과가 나온다고 자동으로 가정하지 않는다.

### 현재 Layer 1 증거와 경계

Layer 1 source는 다음 흐름과 marker를 구현한다.

```text
NVS_READY
BLE_CONTROLLER_ENABLED
BLUEDROID_ENABLED
ADVERTISING_STARTED
ADVERTISING_ACTIVE
```

2026-08-27 현재 실제 build, Flash, Runtime PASS 기록:

- [Layer 1 실행 로그](../layers/layer-1/logs/esp32s3-layer-1-ble-advertising-20260827T122914-KST.log)

이 로그는 `[LAYER-1] ADVERTISING_ACTIVE`까지 증명하지만 `OVER_AIR_SCAN=NOT_VERIFIED`로 끝난다. 스마트폰에서 `ESP32-LAYER-1` 이름과 RSSI가 실제로 갱신되는지는 별도 검증 단계다.

### 현재 Layer 3 증거와 경계

2026-08-27 Board B에 Layer 3를 Flash하고, 별도 전원의 Board A가 송신한
`ESP32-LAYER-2`와 Service UUID를 실제로 수신했다.

- [Layer 3 실행 로그](../layers/layer-3/logs/esp32s3-layer-3-active-scanner-20260827T142229-KST.log)

이 로그의 `TARGET_FOUND`, `SERVICE_MATCH`, `TARGET_RX`, RSSI `-39 dBm`은
두 보드 간 Advertising 수신을 증명한다. 아직 GATT connection, 데이터 relay,
Bluetooth Mesh는 구현하거나 검증하지 않았다.

### 현재 Layer 4 증거와 경계

2026-08-27 Board B에 Layer 4를 Flash하고 다음 local runtime을 확인했다.

- [Layer 4 한 보드 실행 로그](../layers/layer-4/logs/esp32s3-layer-4-dual-role-20260827T144822-KST.log)

이 로그는 GATT Server, connectable Advertising, Active Scanning이 같은
firmware에서 시작됐음을 증명한다. Mac CoreBluetooth scan에서도
`ESP32-L4-B6`와 Service UUID가 over the air로 확인됐다.

- [Layer 4 외부 Advertising 수신 로그](../layers/layer-4/logs/esp32s3-layer-4-over-air-adv-20260827T145200-KST.log)

동일 firmware를 두 ESP32-S3에 Flash한 pair workflow도 양쪽 수신을
확인했다.

```text
[A] PEER_RX local=76 peer=B6
[B] PEER_RX local=B6 peer=76
BIDIRECTIONAL_PEER_RX=yes
PAIR_ADV_SCAN=PASS
PAIR_PEER_RX=PASS
```

- [Layer 4 두 보드 실행 로그](../layers/layer-4/logs/esp32s3-layer-4-dual-role-pair-20260827T145221-KST.log)

이 결과는 두 보드의 직접 Advertising/Scanning을 증명한다. custom packet,
forwarding, 세 보드 Relay, Bluetooth Mesh는 아직 포함하지 않는다.

### 현재 Layer 5 증거와 경계

2026-08-27 동일 Layer 5 firmware를 두 ESP32-S3에 Flash하고, 양쪽 custom
packet 송신과 상대 packet 직접 수신을 확인했다.

```text
[A] PACKET_TX sender=76 seq=2 ... payload=HELLO crc=ok
[B] PACKET_TX sender=B6 seq=2 ... payload=HELLO crc=ok
[A] PACKET_RX_NEW local=76 sender=B6 seq=2 ... payload=HELLO crc=ok rssi=-30
[B] PACKET_RX_NEW local=B6 sender=76 seq=2 ... payload=HELLO crc=ok rssi=-32
BIDIRECTIONAL_PACKET_RX_NEW=yes
PAIR_PACKET_TX=PASS
PAIR_PACKET_RX=PASS
PAIR_PACKET_CRC=PASS
```

- [Layer 5 두 보드 실행 로그](../layers/layer-5/logs/esp32s3-layer-5-packet-node-pair-20260827T150742-KST.log)

Host test에서도 CRC16/CCITT-FALSE 표준 벡터, exact 20-byte wire format,
encode/decode, invalid packet rejection, 32-entry dedup FIFO를 확인했다.

이 결과는 두 보드 사이의 직접 custom Advertising packet 송수신을
증명한다. 수신 packet forwarding, A -> B -> C relay, Bluetooth Mesh는 아직
구현하거나 검증하지 않았다.

### 현재 Layer 6 증거와 경계

2026-08-27 동일 Layer 6 firmware를 두 ESP32-S3에 Flash했다. Node `76`과
`B6`가 각각 origin packet을 송신했고, 상대 보드가 exact `(origin, sequence)`를
direct 수신한 뒤 TTL만 `2 -> 1`로 변경하여 forward했다.

```text
[A] PACKET_TX_ORIGIN local=76 origin=76 seq=1 ttl=2 ... crc=ok
[B] PACKET_RX_DIRECT local=B6 origin=76 via=76 seq=1 ttl=2 ... duplicate=no crc=ok
[B] PACKET_TX_FORWARD local=B6 origin=76 seq=1 ttl=1 ... crc=ok
[B] PACKET_TX_ORIGIN local=B6 origin=B6 seq=1 ttl=2 ... crc=ok
[A] PACKET_RX_DIRECT local=76 origin=B6 via=B6 seq=1 ttl=2 ... duplicate=no crc=ok
[A] PACKET_TX_FORWARD local=76 origin=B6 seq=1 ttl=1 ... crc=ok
BIDIRECTIONAL_DIRECT_RX=yes
BIDIRECTIONAL_FORWARD_TX=yes
PAIR_FORWARD_TX=PASS
TRIPLET_RELAY=NOT_VERIFIED
```

- [Layer 6 두 보드 실행 로그](../layers/layer-6/logs/esp32s3-layer-6-relay-node-pair-20260827T152751-KST.log)

Host test는 TTL 감소 외 field 보존, 변경된 CRC 재생성, TTL 0 차단,
direct/relayed 분류와 path dedup FIFO를 확인했고 ESP-IDF clean build도 PASS했다.

이 결과는 custom forwarding source와 두 실제 보드의 direct RX/forward TX를
증명한다. 세 번째 고유 node `C`에서 같은 identity의
`PACKET_RX_RELAYED origin=A via=B ttl=1`을 받은 것은 아직 증명하지 않았다.
따라서 Layer 6 전체와 물리적 range extension, 표준 Bluetooth Mesh Relay는
아직 완료로 표시하지 않는다.

</details>

## 다음 결과를 기록할 때

날짜, 대상 보드·펌웨어, 실행한 단계, 관찰한 marker, 로그 경로와 미검증 범위를 함께 남긴다. 새 펌웨어를 Flash하면 이전 앱이 교체되므로 과거 기록의 보드 배치를 현재 상태로 가정하지 않는다.

소스 구현 → 호스트 검사 → MCU 빌드 → Flash → 부팅 → 직접 수신 → Relay를 구분한다. 기존 Layer 결과는 팀 통신 모듈의 성공으로 자동 승계하지 않는다.
