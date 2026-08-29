> 이관 원문: `layers/layer-4/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 4: GATT + Advertising + Active Scanning

## 목표

같은 Layer 4 firmware를 두 ESP32-S3에 넣고 각 보드가 동시에 다음 역할을
수행하는지 확인한다.

```text
GATT Server + connectable Advertising + Active Scanning
```

각 보드는 Bluetooth MAC 마지막 byte로 이름과 node ID를 만든다.

```text
ESP32-L4-76  node=76
ESP32-L4-B6  node=B6
```

두 보드는 GATT로 서로 연결하지 않는다. 각자 Advertising하면서 상대의
Advertising과 scan response를 Active Scan으로 수신한다.

## Layer 2/3에서 추가된 것

- Layer 2의 GATT RX Write/TX Read/Notify와 ACK 동작 유지
- Layer 3의 Active Scan 추가
- 한 firmware에서 GATT Server, Advertising, Scanning 동시 실행
- 같은 binary를 사용하는 두 보드의 MAC 기반 identity
- 양쪽 `PEER_RX`가 모두 있어야 성공하는 pair workflow

Custom application packet, TTL, deduplication, forwarding, Relay, Bluetooth
Mesh는 Layer 4에 포함하지 않는다.

## 가장 중요한 검증: 두 보드 함께 Bootload

두 ESP32-S3를 Mac data USB에 동시에 연결하고 휴대폰 BLE connection을
끊는다. 두 port만 연결되어 있으면 자동 선택한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-4
./bootload-pair.sh
```

여러 USB serial 장치가 있으면 port를 명시한다.

```bash
./bootload-pair.sh \
  --port-a /dev/cu.usbmodemXXXX \
  --port-b /dev/cu.usbmodemYYYY
```

스크립트는 firmware를 한 번 build하고 두 보드를 순서대로 flash한 뒤,
두 serial port를 동시에 읽는다.

자동 PASS 조건:

```text
Board A: ESP32-S3 + 16 MB flash
Board B: ESP32-S3 + 16 MB flash
Board A: gatt=yes advertising=yes scanning=yes
Board B: gatt=yes advertising=yes scanning=yes
Board A: PEER_RX peer=<Board B node>
Board B: PEER_RX peer=<Board A node>
```

최종 출력:

```text
BIDIRECTIONAL_PEER_RX=yes
RESULT=PASS
PAIR_ADV_SCAN=PASS
PAIR_GATT_SERVER=PASS
PAIR_PEER_RX=PASS
```

한쪽에서만 `PEER_RX`가 나오면 PASS가 아니다.

## 한 보드만 Bootload

한 보드의 build, flash, GATT/Advertising/Scanning 시작만 확인할 때 사용한다.

```bash
./bootload.sh --port /dev/cu.usbmodemXXXX
```

성공 결과에는 의도적으로 다음 경계가 기록된다.

```text
LOCAL_GATT_ADV_SCAN=PASS
PAIR_PEER_RX=NOT_VERIFIED
```

한 보드 workflow로 두 보드 무선 수신을 증명할 수는 없다.

## 직접 Serial Monitor로 확인

프로젝트 root에서 다음을 실행한다.

```bash
./scripts/monitor.sh 4
```

두 USB serial port가 있으면 하나를 명시한다.

```bash
ESP_PORT=/dev/cu.usbmodemXXXX ./scripts/monitor.sh 4
```

정상 startup marker:

```text
[LAYER-4] NODE_READY id=XX name=ESP32-L4-XX
[LAYER-4] GATT_SERVICE_READY
[LAYER-4] ADVERTISING_STARTED
[LAYER-4] SCANNING_STARTED mode=active
[LAYER-4] DUAL_ROLE_READY
```

상대 보드 수신 marker:

```text
[LAYER-4] PEER_FOUND name=ESP32-L4-YY node=YY rssi=-41
[LAYER-4] PEER_RX local=XX peer=YY count=1 rssi=-41
```

1초 heartbeat:

```text
[LAYER-4] DUAL_ROLE_ACTIVE ... gatt=yes advertising=yes scanning=yes ...
```

Serial Monitor는 `Ctrl + ]`로 종료한다.

## 수동 동작 테스트

### 1. Advertising + Scanning

두 보드 전원을 켜면 각 보드의 `peer_count`가 증가해야 한다.

한 보드의 전원을 끄면 다른 보드는 계속 `DUAL_ROLE_ACTIVE`를 출력하지만
`peer_count` 증가가 멈춘다. 다시 켜면 `PEER_RX`와 count 증가가 재개된다.

### 2. 휴대폰 GATT

nRF Connect 또는 LightBlue에서 `ESP32-L4-XX` 하나를 선택한다.

1. Connect
2. Service `...0001` 확인
3. RX `...0002`에 UTF-8 `HELLO` Write
4. TX `...0003` Read에서 `ACK:HELLO` 확인
5. TX Notification 활성화
6. RX에 다시 `HELLO` Write
7. `ACK:HELLO` Notification 확인
8. Disconnect

연결된 보드의 예상 로그:

```text
CONNECTED ... scan=active
RX_WRITE len=5 value=HELLO
TX_NOTIFY len=9 value=ACK:HELLO
DISCONNECTED ... scan=active
ADVERTISING_RESTARTED
```

휴대폰 연결 중 해당 보드의 `advertising=no`는 정상이다. Active Scanning은
계속 `scanning=yes`여야 하며 disconnect 후 Advertising이 다시 시작되어야
한다.

## 증거 경계

Layer 4 pair PASS가 증명하는 것:

- 두 실제 ESP32-S3에 같은 firmware flash
- 두 보드 모두 GATT Server 준비
- 두 보드 모두 connectable Advertising 시작
- 두 보드 모두 Active Scanning 시작
- Board A가 Board B를 실제 수신
- Board B가 Board A를 실제 수신

아직 증명하지 않는 것:

- custom packet 송수신
- received packet forwarding
- A → B → C Relay
- Bluetooth Mesh
