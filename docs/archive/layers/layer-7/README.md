> 이관 원문: `layers/layer-7/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 7 — Standard Bluetooth Mesh Final Node

Layer 7은 세 ESP32-S3에 **같은 firmware**를 설치하는 최종 학습 Layer다.
각 보드는 다음 세 SIG Model을 한 Element에 동시에 가진다.

```text
Element 0
├── Configuration Server
├── Generic OnOff Server
└── Generic OnOff Client
```

Layer 6의 custom Advertising packet/CRC/TTL forwarding을 확장한 것이 아니다.
Layer 7에서는 ESP-BLE-MESH 표준 stack이 암호화, Network TTL, duplicate 처리와
Relay를 담당한다.

## 1. 한 보드에 설치하기

여러 보드가 연결되어 있다면 포트를 반드시 지정한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-7
./bootload.sh --port /dev/cu.usbmodemXXXXXXXX
```

처음부터 새 Mesh network로 실험할 때만 `--erase`를 사용한다. 이 옵션은
Provisioning keys와 Model 설정이 들어 있는 NVS까지 지운다.

```bash
./bootload.sh --port /dev/cu.usbmodemXXXXXXXX --erase
```

그냥 다시 Flash하면 기존 Mesh 설정은 유지된다.

## 2. 세 보드에 같은 image 설치하기

세 포트가 연결된 경우:

```bash
./bootload-triplet.sh --erase
```

다른 serial 장치도 함께 보이면 명시한다.

```bash
./bootload-triplet.sh \
  --port-a /dev/cu.usbmodemA \
  --port-b /dev/cu.usbmodemB \
  --port-c /dev/cu.usbmodemC \
  --erase
```

이 결과의 `TRIPLET_BUILD_FLASH_BOOT=PASS`는 세 보드의 build/flash/boot와
서로 다른 identity만 증명한다. iPhone discovery, Provisioning, group message,
Relay는 여전히 `NOT_VERIFIED`다.

## 3. iPhone nRF Mesh 설정

Provisioner는 Nordic Semiconductor의 **nRF Mesh** iPhone 앱을 사용한다.
앱 화면 명칭은 버전에 따라 조금 다를 수 있지만 설정해야 하는 Mesh 상태는
아래와 같다.

### A. 세 Node Provisioning

1. nRF Mesh에서 새 network를 만든다.
2. `Add node` 또는 scan을 시작한다.
3. `ESP32-MESH-XX`를 선택해 Provision한다.
4. serial에서 `PROVISIONING_LINK_OPEN bearer=PB-GATT`와
   `PROVISIONING_COMPLETE`를 확인한다.
5. 나머지 두 보드도 반복한다.
6. 세 Node의 unicast address가 서로 다른지 기록한다.

Provisioning만 성공한 상태에서는 아직 `on`/`off`를 보낼 수 없다.

### B. AppKey와 Model 설정 — 세 Node 모두

각 Node의 Element 0에서 다음을 설정한다.

1. 같은 AppKey를 Node에 Add한다.
2. Generic OnOff Server에 그 AppKey를 Bind한다.
3. Generic OnOff Client에도 그 AppKey를 Bind한다.
4. Generic OnOff Server Subscription에 group `0xC000`을 추가한다.
5. Generic OnOff Client Publication address를 `0xC000`으로 설정한다.
6. Client Publication AppKey를 같은 AppKey로 선택하고 TTL을 `7`로 둔다.
7. GATT Proxy가 Enabled인지 확인한다.

완료 로그의 핵심 marker:

```text
[LAYER-7] APPKEY_ADDED
[LAYER-7] MODEL_APP_BOUND ... model=GEN_ONOFF_SERVER
[LAYER-7] MODEL_APP_BOUND ... model=GEN_ONOFF_CLIENT
[LAYER-7] GROUP_SUBSCRIBED ... address=0xC000
[LAYER-7] CLIENT_PUBLICATION_READY address=0xC000
```

### C. Relay 설정

처음에는 세 보드 모두 Relay가 Disabled다. 중간 보드 B의 Configuration
Server에서만 Relay를 Enabled로 설정한다. 권장 학습값은 retransmit count 2,
interval 20 ms다.

```text
[LAYER-7] RELAY_STATE_CHANGED state=enabled
```

## 4. OnOff 보내기

세 포트를 한 화면에서 보고 command도 보내려면:

```bash
./monitor-triplet.sh PORT_A PORT_B PORT_C
```

실행 후 stdin에 다음처럼 입력한다.

```text
A:status
A:on-unack
A:off-unack
B:on
quit
```

지원 명령:

- `on`, `off`: acknowledged Generic OnOff Set
- `on-unack`, `off-unack`: unacknowledged Generic OnOff Set
- `tx-low`: 이 노드의 BLE Advertising TX power를 일시적으로 `-24 dBm`으로 낮춤
- `tx-normal`: 부팅할 때 기록한 정상 Advertising TX power로 복구
- `status`: 현재 Provisioning/key/publication/Relay/OnOff/TX power 상태
- `factory-reset`: local Mesh state 삭제 후 재부팅

`tx-low`는 NVS에 저장하지 않는 relay-test 전용 상태다. 현재 실행 중인 노드의
Mesh Advertising bearer 전체에 적용되며, `tx-normal` 또는 재부팅으로 정상
출력에 복구된다. `TX_POWER_SET` 로그의 `requested_dbm`과 `applied_dbm`이
일치하는지 확인한다.

Group `0xC000`에는 여러 Server가 응답 충돌을 만들 수 있으므로 첫 group
검증에는 `on-unack`/`off-unack`가 더 단순하다. 성공 시 source에는
`ONOFF_TX_ACCEPTED`, subscribed receiver에는 `ONOFF_RX`가 보인다.

Server는 acknowledged Set에만 요청 source로 Generic OnOff Status를 한 번
응답한다. Unacknowledged Set은 상태만 변경하며 응답하지 않는다. Server
Publication은 Configuration Client가 별도로 설정했을 때 사용하는 독립 경로이므로,
Set을 받을 때마다 추가 Status Publication을 시도하지 않는다.

자기 자신도 구독한 group으로 전송할 때 ESP-BLE-MESH stack에서
`No outbound bearer found, inbound bearer 1` warning이 한 번 보일 수 있다.
현재 A/B/C의 실제 `ONOFF_RX`와 acknowledged Status 응답은 정상 동작하며,
이 warning은 제거된 application-level `ONOFF_STATUS_PUBLISH_FAILED`와는 별개다.

## 5. Relay를 정확히 검증하는 방법

`ONOFF_RX` callback에는 원래 source/destination은 보이지만 바로 직전 Relay
주소(`via`)는 보이지 않는다. 따라서 가까이 둔 세 보드가 모두 수신한 것만으로
Relay 성공이라고 하면 안 된다.

같은 A/B/C 위치와 설정을 유지하고 A와 C의 직접 경로만 차단 또는 충분히
감쇠한 뒤 비교한다.

1. A-B는 수신 가능, B-C도 수신 가능, A-C 직접 수신은 불가능하게 만든다.
2. B Relay Disabled 상태에서 A가 동일 command를 여러 번 보낸다.
3. C가 받지 않는 것을 기록한다.
4. 위치를 바꾸지 않고 B Relay Enabled로 변경한다.
5. A가 같은 종류의 command를 여러 번 보낸다.
6. C의 반복 수신을 기록한다.

이 OFF/ON 대조 실험을 통과해야 `TRIPLET_RELAY=PASS`로 기록할 수 있다.

### 세 보드를 Mac에 연결한 저출력 Relay 실험

현재 UART command가 들어가는 D6를 송신기로 사용한다.

```text
D6/0x0005 (tx-low 송신) -> B/0x0004 (Relay) -> A/0x0003 (수신)
```

1. D6와 B는 가깝게 두고 A는 USB cable 범위에서 최대한 멀리 둔다.
2. nRF Mesh에서 B의 Relay를 Disabled로 설정한다.
3. D6에서 `tx-low`, `status`, `on-unack`을 차례로 실행한다.
4. B는 `ONOFF_RX`, A는 미수신하는 기준 상태를 확인한다.
5. 위치를 바꾸지 않고 nRF Mesh에서 B의 Relay를 Enabled로 설정한다.
6. D6에서 `off-unack`을 실행한다.
7. A가 `ONOFF_RX`를 수신하면 B Relay 경로가 성립한다.
8. 실험 후 D6에서 `tx-normal`, `status`를 실행한다.

`-24 dBm`은 직접 경로를 약하게 만들지만 A의 직접 수신을 수학적으로 보장해
차단하지는 않는다. Relay Disabled에서도 A가 수신하면 A를 조금 더 멀리 두거나
부분 차폐한 뒤, 같은 위치에서 OFF/ON 대조를 다시 수행한다.

## 현재 검증 상태

- serial parser host test: PASS
- ESP-IDF v5.5.5 ESP32-S3 build: PASS
- 실제 board flash/boot: PASS (A/B/C, firmware SHA `492c7d279` 기준)
- iPhone discovery/Provisioning/configuration: PASS
- three-node one-hop group OnOff: PASS
  (`0x0005` -> `0xC000` -> `0x0003/0x0004/0x0005`, unacknowledged ON 및 acknowledged OFF)
- acknowledged Status response: PASS (`0x0003/0x0004/0x0005` -> `0x0005`)
- redundant Server Status Publication warning 제거: PASS
- relay-test TX power control: PASS
  (`tx-low`: `-24 dBm` read-back, `tx-normal`: `+9 dBm` read-back)
- 현재 책상 배치의 low-power direct reception: A/B 모두 수신
  (직접 경로가 남아 있어 Relay 증거로 사용하지 않음)
- A/B를 sender로 사용한 반대 방향 serial test: NOT_VERIFIED
  (현재 native USB Serial/JTAG는 log output만 제공하고 command input은 UART0 사용)
- controlled Relay comparison: NOT_VERIFIED

설계와 증거 경계는
[Layer 7 design](../../designs/specs/2026-08-27-layer-7-standard-mesh-final-design.md)에 정리되어 있다.
