# ESP32-S3 Layer 6: Symmetric Advertising Forwarding

## 목표

Layer 5의 GATT Server, connectable Advertising, Active Scan, 20-byte custom
packet을 유지하면서 모든 ESP32-S3가 같은 firmware로 packet을 만들고,
받고, TTL이 남아 있으면 다시 Advertising하는 대칭형 relay node가 된다.

```text
A: origin packet (origin=A, sequence=N, ttl=2)
             |
B: direct RX -> forward (origin=A, sequence=N, ttl=1)
             |
C: relayed RX (origin=A, via=B, sequence=N, ttl=1)
```

이것은 일반 BLE Advertising 위에 만든 학습용 custom forwarding protocol이다.
표준 Bluetooth Mesh의 Provisioning, AppKey, Model, Relay Feature가 아니다.

## Layer 5에서 추가된 것

- 모든 보드가 originator, scanner, forwarder 역할을 동시에 수행
- 수신 packet의 `sender`를 original origin으로 그대로 유지
- `sequence`, `recipient`, `payload`를 그대로 유지
- forward할 때 `ttl`만 1 감소시키고 CRC16을 다시 생성
- `ttl == 0` packet은 수신할 수 있지만 더 이상 forward하지 않음
- callback은 scan 결과를 RX queue로 복사하고, decode/forward는 worker task에서 수행
- origin과 forward packet을 TX queue로 직렬화
- deterministic 80–200 ms forwarding delay로 동시 재전송 충돌 가능성을 낮춤
- logical dedup `(origin, sequence)`과 path dedup `(origin, sequence, via)`를 분리

## Packet identity와 `via`

20-byte wire packet은 Layer 5와 같다. Byte 3의 `sender`는 forwarding 중에도
바뀌지 않으며 여기서는 `origin`이라는 의미로 사용한다. `via`는 wire packet의
field가 아니라 scan report를 보낸 Bluetooth address의 마지막 byte다.

따라서 다음을 구분할 수 있다.

```text
origin == via  -> PACKET_RX_DIRECT
origin != via  -> PACKET_RX_RELAYED
```

forward 전후의 차이는 다음뿐이다.

| Field | Origin TX | Forward TX |
|---|---:|---:|
| origin/sender | `A` | `A` 유지 |
| sequence | `N` | `N` 유지 |
| recipient | `FF` | `FF` 유지 |
| payload | `HELLO` | `HELLO` 유지 |
| ttl | `2` | `1` |
| CRC | origin bytes 기준 | 변경된 TTL 기준 재생성 |

## Host tests

보드 없이 packet codec, CRC, TTL 처리, field 보존, direct/relayed 분류,
logical/path dedup FIFO를 시험한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-6
./host-tests/run-tests.sh
```

성공 marker:

```text
LAYER_PACKET_HOST_TESTS=PASS
LAYER_RELAY_HOST_TESTS=PASS
```

## 한 보드 Bootload

```bash
./bootload.sh --port /dev/cu.usbmodemXXXX
```

성공 시 local runtime과 origin TX만 증명한다.

```text
LOCAL_GATT_ADV_SCAN=PASS
LOCAL_ORIGIN_TX=PASS
PAIR_DIRECT_RX=NOT_VERIFIED
PAIR_FORWARD_TX=NOT_VERIFIED
TRIPLET_RELAY=NOT_VERIFIED
```

## 두 보드 Bootload 및 Forward 확인

Mac에 두 보드만 연결되어 있으면 자동 선택한다.

```bash
./bootload-pair.sh
```

또는 port를 직접 지정한다.

```bash
./bootload-pair.sh \
  --port-a /dev/cu.usbmodemXXXX \
  --port-b /dev/cu.usbmodemYYYY
```

두 보드 각각에서 origin TX가 있어야 하고, 그 exact `(origin, sequence)`가
상대 보드의 `PACKET_RX_DIRECT duplicate=no`와 `PACKET_TX_FORWARD ttl=1`까지
이어져야 PASS한다. 세 번째 보드가 없으므로 relayed RX는 증명하지 않는다.

```text
BIDIRECTIONAL_DIRECT_RX=yes
BIDIRECTIONAL_FORWARD_TX=yes
RESULT=PASS
PAIR_ORIGIN_TX=PASS
PAIR_DIRECT_RX=PASS
PAIR_FORWARD_TX=PASS
TRIPLET_RELAY=NOT_VERIFIED
```

## 세 보드 Relay 확인

Mac에 세 보드를 동시에 연결한 뒤 실행한다.

```bash
./bootload-triplet.sh
```

또는 세 port를 모두 지정한다.

```bash
./bootload-triplet.sh \
  --port-a /dev/cu.usbmodemXXXX \
  --port-b /dev/cu.usbmodemYYYY \
  --port-c /dev/cu.usbmodemZZZZ
```

스크립트는 세 보드를 profile하고 동일한 binary를 flash한 뒤, 다음 네 로그가
하나의 exact `(origin, sequence)` identity로 연결되는지 검사한다.

1. A의 `PACKET_TX_ORIGIN ... ttl=2`
2. B의 `PACKET_RX_DIRECT ... via=A ... ttl=2 duplicate=no`
3. B의 `PACKET_TX_FORWARD ... origin=A ... ttl=1`
4. C의 `PACKET_RX_RELAYED ... origin=A via=B ... ttl=1`

성공 marker:

```text
TRIPLET_RELAY_CHAIN=yes
TRIPLET_RELAY=PASS
RESULT=PASS
```

세 보드가 모두 같은 공간에 있으면 C가 A의 direct copy도 함께 받을 수 있다.
이 검증은 direct copy의 부재가 아니라, B가 만든 별도의 relayed copy를 C가
수신했다는 것을 `via=B`로 증명한다. 물리적 range extension 시험은 별도의
차폐 또는 거리 조건이 필요하다.

## 주요 serial marker

```text
[LAYER-6] NODE_READY id=XX name=ESP32-L6-XX
[LAYER-6] PACKET_TX_ORIGIN local=XX origin=XX seq=N ttl=2 ... crc=ok
[LAYER-6] PACKET_RX_DIRECT local=YY origin=XX via=XX seq=N ttl=2 duplicate=no crc=ok
[LAYER-6] PACKET_FORWARD_QUEUED local=YY origin=XX seq=N ttl=1 delay_ms=...
[LAYER-6] PACKET_TX_FORWARD local=YY origin=XX seq=N ttl=1 ... crc=ok
[LAYER-6] PACKET_RX_RELAYED local=ZZ origin=XX via=YY seq=N ttl=1 ... crc=ok
[LAYER-6] RELAY_NODE_ACTIVE ... gatt=yes advertising=yes scanning=yes ...
```

직접 monitor할 때:

```bash
ESP_PORT=/dev/cu.usbmodemXXXX ./scripts/monitor.sh 6
```

## GATT 회귀 확인

nRF Connect 또는 LightBlue에서 `ESP32-L6-XX`에 연결하면 Layer 2와 같은
Service `...0001`, RX `...0002`, TX `...0003`을 볼 수 있다. RX Write,
TX Read, Notification `ACK:<value>`, disconnect 후 Advertising 재시작도
그대로 유지된다.

## 증거 경계

Host tests와 build 성공은 source/ELF 검증이다. 실제 board flash와 runtime은
각 bootload script의 PASS log가 별도로 필요하다. 특히 Layer 6 최종 relay는
`bootload-triplet.sh`의 세 실제 board identity chain이 있어야만 완료다.
