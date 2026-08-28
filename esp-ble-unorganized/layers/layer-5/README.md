# ESP32-S3 Layer 5: Custom Advertising Packet

## 목표

Layer 4의 GATT Server + connectable Advertising + Active Scan을 유지하면서,
두 ESP32-S3가 동일한 20-byte application packet을 Advertising으로 보내고
상대 packet을 검증·중복 제거해 수신하는지 확인한다.

```text
Board A PACKET_TX -> Board B PACKET_RX_NEW
Board B PACKET_TX -> Board A PACKET_RX_NEW
```

이 단계는 연결 없는 일반 BLE Advertising packet 실습이다. 수신 packet을
다시 보내는 forwarding, 세 보드 Relay, 표준 Bluetooth Mesh는 아직 없다.

## Layer 4에서 추가된 것

- primary Advertising의 Manufacturer Specific Data에 20-byte packet 포함
- CRC16/CCITT-FALSE로 bytes 0..17 검증
- `(sender, sequence)`를 key로 하는 32-entry FIFO dedup
- scan callback에서는 16-entry FreeRTOS queue에 packet을 복사만 함
- worker task에서 decode, CRC, recipient, dedup, payload 처리
- 연결되지 않은 동안 1초마다 sequence를 증가시켜 새 packet Advertising
- host에서 packet codec과 dedup을 ESP-IDF 없이 반복 시험

Layer 2부터의 GATT RX Write, TX Read, ACK, Notification도 그대로 유지한다.

## 20-byte packet 형식

| Byte | Field | Layer 5 값 |
|---:|---|---|
| 0 | version | `1` |
| 1 | type | `HELLO = 1` |
| 2 | ttl | `2` |
| 3 | sender | Bluetooth MAC 마지막 byte |
| 4 | recipient | broadcast `0xFF` |
| 5..6 | sequence | little-endian, 1부터 증가, 0은 건너뜀 |
| 7 | payload length | `5` |
| 8..17 | payload | `HELLO` + zero padding |
| 18..19 | CRC16 | CCITT-FALSE, little-endian |

CRC parameter는 polynomial `0x1021`, initial value `0xFFFF`이고 bytes
0..17을 계산한다. Advertising의 Company ID는 학습용 `0xFFFF`다.

## Host packet test

보드 없이 CRC, exact wire format, encode/decode, invalid packet rejection,
dedup FIFO를 먼저 확인한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-5
./host-tests/run-tests.sh
```

성공 marker:

```text
LAYER_PACKET_HOST_TESTS=PASS
```

## 가장 중요한 검증: 두 보드 함께 Bootload

두 ESP32-S3를 Mac data USB에 동시에 연결하고 실행한다. 두 port만 있으면
자동 선택한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-5
./bootload-pair.sh
```

port를 직접 지정할 수도 있다.

```bash
./bootload-pair.sh \
  --port-a /dev/cu.usbmodemXXXX \
  --port-b /dev/cu.usbmodemYYYY
```

스크립트는 한 번 build하고 두 보드를 순서대로 flash한 뒤 두 serial port를
동시에 읽는다. 양쪽 모두 local readiness, `PACKET_TX`, 상대 node의
`PACKET_RX_NEW ... payload=HELLO crc=ok`가 있어야 PASS한다.

```text
BIDIRECTIONAL_PACKET_RX_NEW=yes
RESULT=PASS
PAIR_ADV_SCAN=PASS
PAIR_GATT_SERVER=PASS
PAIR_PACKET_TX=PASS
PAIR_PACKET_RX=PASS
PAIR_PACKET_CRC=PASS
```

## 한 보드만 Bootload

한 보드의 build, flash, local GATT/ADV/SCAN과 packet 송신까지만 확인한다.

```bash
./bootload.sh --port /dev/cu.usbmodemXXXX
```

성공해도 pair 수신은 의도적으로 미검증으로 남는다.

```text
LOCAL_GATT_ADV_SCAN=PASS
LOCAL_PACKET_TX=PASS
PAIR_PACKET_RX=NOT_VERIFIED
```

## Serial marker

```text
[LAYER-5] NODE_READY id=XX name=ESP32-L5-XX
[LAYER-5] GATT_SERVICE_READY
[LAYER-5] ADVERTISING_STARTED
[LAYER-5] SCANNING_STARTED mode=active
[LAYER-5] PACKET_TX sender=XX seq=1 ... payload=HELLO crc=ok
[LAYER-5] PACKET_RX_NEW local=XX sender=YY seq=... payload=HELLO crc=ok
[LAYER-5] PACKET_NODE_ACTIVE ... gatt=yes advertising=yes scanning=yes ...
```

직접 monitor할 때:

```bash
ESP_PORT=/dev/cu.usbmodemXXXX ./scripts/monitor.sh 5
```

## GATT 회귀 확인

nRF Connect 또는 LightBlue에서 `ESP32-L5-XX`에 연결하면 Layer 2와 같은
Service `...0001`, RX `...0002`, TX `...0003`을 볼 수 있다. RX에 `HELLO`를
Write하면 TX Read 또는 활성화된 Notification에서 `ACK:HELLO`를 확인한다.
연결 중 해당 보드의 Advertising packet update는 멈추고 Active Scan은
계속된다. Disconnect 후 Advertising이 다시 시작된다.

## 증거 경계

두 보드 pair PASS가 증명하는 것:

- 같은 Layer 5 firmware의 두 실제 ESP32-S3 flash/runtime
- 양쪽 GATT Server + connectable Advertising + Active Scan
- 양쪽 custom packet 송신
- 양쪽에서 상대 sender/sequence/payload 수신
- 수신 packet의 CRC 성공과 `(sender, sequence)` dedup 적용

아직 증명하지 않는 것:

- 수신 packet forwarding
- A -> B -> C relay
- 표준 Bluetooth Mesh Provisioning, key, Model 또는 Relay
