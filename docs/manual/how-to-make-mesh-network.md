> 이관 원문: `manual/how-to-make-mesh-network.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# How to Make a Bluetooth Mesh Network with Three ESP32-S3 Boards

이 문서는 세 대의 ESP32-S3에 같은 Layer 7 firmware를 설치하고, iPhone의 **nRF Mesh** 앱으로 하나의 표준 Bluetooth Mesh network를 만든 과정을 재현하기 위한 기록이다.

최종 목표는 세 Node가 같은 NetKey와 AppKey를 사용하고, 각 Node의 Generic OnOff Client가 group `0xC000`으로 보내며, 각 Node의 Generic OnOff Server가 그 group을 수신하도록 만드는 것이다.

> 이 문서는 2026-08-27~28에 실제로 진행한 절차와 결과를 기준으로 한다. Build, Flash, Boot, Provisioning, Configuration, one-hop group 통신은 확인했다. 실내 거리에서는 직접 경로가 계속 살아 있어 통제된 Relay OFF/ON 비교 실험은 보류했다.

## 1. 완성된 구조

세 보드에는 서로 다른 Client/Server firmware를 설치하지 않는다. **세 보드 모두 같은 symmetric Layer 7 firmware**를 사용한다.

```text
각 ESP32-S3의 Element 0
├── Configuration Server
├── Generic OnOff Server
└── Generic OnOff Client
```

Mesh 설정은 다음과 같다.

```text
                         Group 0xC000
               ┌────────────┼────────────┐
               │            │            │
       0x0003-ESP32-76  0x0004-ESP32-B6  0x0005-ESP32-D6
          Client/Server    Client/Server    Client/Server
               │            │            │
               └──── 같은 NetKey와 AppKey ────┘
```

- Client publication: `0xC000`
- Server subscription: `0xC000`
- Network TTL: `7`
- GATT Proxy: Enabled
- Relay: 기본값 Disabled, Relay 실험 때 중간 Node에서만 Enabled

Layer 6의 custom Advertising forwarding과 Layer 7의 Bluetooth Mesh Relay는 다른 방식이다. Layer 7에서는 ESP-BLE-MESH stack이 암호화, 주소, Sequence Number, TTL, duplicate 처리와 Relay를 담당한다.

## 2. 준비물

- ESP32-S3 보드 3대
- 데이터 전송이 가능한 USB cable 또는 보드별 독립 전원
- macOS와 ESP-IDF `v5.5.5`
- iPhone
- Nordic Semiconductor의 nRF Mesh 앱
- 이 workspace의 [Layer 7 project](../layers/layer-7/README.md)

세 Node가 모두 Mac USB에 연결될 필요는 없다. Firmware 설치와 serial log 확인에는 USB data connection이 필요하지만, 설치 후 Mesh에 참여할 때는 충전기나 power bank의 전원만 받아도 된다.

## 3. 보드 식별 정보

Firmware는 Bluetooth MAC address의 마지막 byte를 사용해 `ESP32-MESH-XX` 이름을 만든다. nRF Mesh 안의 local Node 이름은 아래처럼 Unicast Address와 suffix를 함께 적어 두면 혼동이 적다.

| 물리 라벨 | Unicast Address | Firmware BLE 이름 | nRF Mesh 권장 이름 |
| --- | --- | --- | --- |
| Node-A | `0x0003` | `ESP32-MESH-76` | `0x0003-ESP32-76` |
| Node-B | `0x0004` | `ESP32-MESH-B6` | `0x0004-ESP32-B6` |
| Node-C | `0x0005` | `ESP32-MESH-D6` | `0x0005-ESP32-D6` |

nRF Mesh에서 이름을 바꾸는 것은 iPhone 앱 안의 local label을 바꾸는 작업이다. Unicast Address나 firmware가 만드는 `ESP32-MESH-XX` identity는 바뀌지 않는다.

macOS의 `/dev/cu.*` port 이름은 USB를 다시 연결하면 달라질 수 있다. 영구 ID처럼 기록하지 말고 매 실행마다 다시 확인한다.

```bash
find /dev -maxdepth 1 \
  \( -name 'cu.usbmodem*' -o -name 'cu.usbserial*' \
  -o -name 'cu.SLAB_USBtoUART*' -o -name 'cu.wchusbserial*' \) \
  -print | sort
```

## 4. 세 보드에 Layer 7 firmware 설치

### 방법 A: 한 대씩 설치

한 보드만 연결한 상태에서 다음을 실행한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-7
./bootload.sh --port /dev/cu.usbmodemXXXXXXXX
```

`bootload.sh`가 다음 작업을 한 번에 수행한다.

1. ESP-IDF toolchain 확인
2. Serial device 존재 확인
3. ESP32-S3와 16 MB Flash profile 확인
4. `idf.py build`
5. Firmware Flash
6. 재부팅 후 Layer 7 runtime marker 확인
7. `layers/layer-7/logs/`에 timestamp가 포함된 log 저장

정상 종료의 핵심 결과는 다음과 같다.

```text
LOCAL_RUNTIME_MARKERS=PASS
RESULT=PASS
BUILD=PASS
FLASH=PASS
RUNTIME=PASS
```

### 방법 B: 세 대를 동시에 연결해 설치

Mac에서 정확히 세 serial port만 보인다면 다음 명령을 사용할 수 있다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-7
./bootload-triplet.sh
```

다른 serial 장치도 연결되어 있다면 세 port를 명시한다.

```bash
./bootload-triplet.sh \
  --port-a /dev/cu.usbmodemA \
  --port-b /dev/cu.usbmodemB \
  --port-c /dev/cu.usbmodemC
```

성공 시 다음 결과를 확인한다.

```text
TRIPLET_DISTINCT_IDENTITIES=PASS
TRIPLET_BUILD_FLASH_BOOT=PASS
```

이 PASS는 세 보드의 Build, Flash, Boot와 서로 다른 identity만 증명한다. 아직 Mesh network 가입이나 message 전달을 증명하지는 않는다.

### `--erase`를 사용하는 경우

처음부터 완전히 새로운 Mesh network를 만들 때만 `--erase`를 붙인다.

```bash
./bootload-triplet.sh --erase
```

`--erase`는 Flash 전체와 NVS에 저장된 NetKey, AppKey, Unicast Address, Model 설정을 삭제한다. 이미 Provisioning한 network를 유지하려면 사용하지 않는다. 일반 Flash는 기존 Mesh 설정을 유지한다.

## 5. iPhone에서 새 Mesh network 만들기

1. iPhone에서 nRF Mesh를 연다.
2. 새 Mesh network를 만든다.
3. Network 이름을 알아보기 쉽게 지정한다.
4. Network Key가 하나 생성되었는지 확인한다.

이때 iPhone은 **Provisioner**다. ESP32는 아직 network에 가입하지 않은 **Unprovisioned Node**다.

Provisioning은 단순 BLE 연결이 아니다. ESP32에 NetKey, DevKey, Unicast Address와 Mesh network 상태를 전달해 정식 구성원으로 등록하는 절차다.

## 6. 세 Node Provisioning

각 보드에 전원을 넣고 다음 절차를 세 번 반복한다.

1. nRF Mesh에서 `Add node` 또는 scanner를 연다.
2. `ESP32-MESH-76`, `ESP32-MESH-B6`, `ESP32-MESH-D6` 중 하나를 선택한다.
3. `Provision`을 실행한다.
4. Provisioning이 끝나면 앱에서 할당된 Unicast Address를 기록한다.
5. Node 이름을 `0x0003-ESP32-76` 형식으로 바꾼다.

Serial log를 볼 수 있다면 다음 marker를 확인한다.

```text
[LAYER-7] PROVISIONING_LINK_OPEN bearer=PB-GATT
[LAYER-7] PROVISIONING_COMPLETE net_idx=0x0000 primary_addr=0x....
```

이번 구성에서 확정한 주소는 다음과 같다.

```text
0x0003 = ESP32-MESH-76
0x0004 = ESP32-MESH-B6
0x0005 = ESP32-MESH-D6
```

Provisioning만 완료하면 세 Node가 같은 Network Key를 갖게 되지만, Generic OnOff application message를 아직 보낼 수는 없다. 다음 Configuration 단계가 필요하다.

## 7. 하나의 AppKey 만들기

nRF Mesh의 network 설정에서 **Application Key를 하나만 만든다**.

- 이번 network의 NetKey index: `0x0000`
- 이번 network의 AppKey index: `0x0000`
- 세 Node와 모든 Generic OnOff Model에서 이 동일한 AppKey object를 선택한다.

nRF Mesh가 AppKey의 실제 key bytes를 자동 생성하므로 사용자가 hex value를 직접 입력하지 못하는 것은 정상이다. 중요한 것은 세 개의 별도 AppKey를 만드는 것이 아니라, network에 만든 **같은 AppKey 하나**를 각 Node에 Add하고 Model에 Bind하는 것이다.

## 8. 각 Node의 Model 설정

다음 설정을 `0x0003`, `0x0004`, `0x0005`에 모두 반복한다.

### 8.1 AppKey를 Node에 Add

1. Node의 Configuration 화면을 연다.
2. `Application Keys`에서 앞서 만든 AppKey를 Add한다.
3. 성공 후 해당 Node가 AppKey index `0x0000`을 갖는지 확인한다.

```text
[LAYER-7] APPKEY_ADDED net_idx=0x0000 app_idx=0x0000
```

### 8.2 Server와 Client 모두 같은 AppKey에 Bind

Element 0에서 각각 다음 Model을 열고 같은 AppKey를 Bind한다.

1. Generic OnOff Server
2. Generic OnOff Client

```text
[LAYER-7] MODEL_APP_BOUND ... model=GEN_ONOFF_SERVER app_idx=0x0000
[LAYER-7] MODEL_APP_BOUND ... model=GEN_ONOFF_CLIENT app_idx=0x0000
```

Server만 Bind하면 이 Node가 message를 받을 수 있어도 Client로 보낼 준비는 끝나지 않는다. Client만 Bind하면 반대 문제가 생긴다. 세 Node가 모두 송신자와 수신자가 될 수 있도록 두 Model을 모두 Bind한다.

### 8.3 Server Subscription 설정

Generic OnOff Server의 Subscription에 group `0xC000`을 추가한다.

```text
[LAYER-7] GROUP_SUBSCRIBED model=GEN_ONOFF_SERVER address=0xC000
```

Subscription은 “이 Server가 어느 group으로 온 message를 받을 것인가”를 정한다.

### 8.4 Client Publication 설정

Generic OnOff Client의 Publication을 다음처럼 설정한다.

| 항목 | 값 |
| --- | --- |
| Publish Address | `0xC000` |
| AppKey | 앞에서 만든 동일 AppKey, index `0x0000` |
| TTL | `7` |

```text
[LAYER-7] CLIENT_PUBLICATION_READY address=0xC000 app_idx=0x0000 ttl=7
```

Publication은 “이 Client가 목적지를 따로 지정하지 않고 보낼 때 어느 주소로 보낼 것인가”를 정한다.

### 8.5 Proxy 확인

GATT Proxy를 Enabled로 둔다. iPhone은 같은 Network Key를 아는 Proxy Node 하나에 연결하면 그 Proxy를 통해 Mesh configuration message를 다른 Node로 전달할 수 있다.

Node-C를 설정한다고 반드시 Node-C 자체에 Proxy connection을 만들어야 하는 것은 아니다. 같은 network의 Node-A Proxy에 연결된 상태에서도 Node-C를 configure할 수 있다.

## 9. 설정 상태 확인

Serial command 입력이 가능한 보드에서 `status`를 실행한다.

```text
status
```

정상 구성이라면 다음 field를 확인한다.

```text
[LAYER-7] STATUS name=ESP32-MESH-D6
  provisioned=yes
  primary=0x0005
  net_idx=0x0000
  app_idx=0x0000
  appkey_bound=yes
  client_pub=0xC000
  server_sub=0xC000
  relay=disabled 또는 enabled
```

`appkey_bound=yes`, `client_pub=0xC000`, `server_sub=0xC000`이 모두 보여야 이 Node의 Generic OnOff 송수신 설정이 완료된 것이다.

## 10. Group OnOff 검증

세 보드의 log를 동시에 볼 수 있으면 다음 monitor를 사용한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-7
./monitor-triplet.sh PORT_A PORT_B PORT_C
```

입력 형식은 `보드 라벨:명령`이다.

```text
C:status
C:on-unack
C:off
quit
```

처음 group 전송을 확인할 때는 `on-unack` 또는 `off-unack`가 단순하다. Acknowledged message는 group의 여러 Server가 Status를 동시에 답하므로 log가 더 많아진다.

송신 Node에서는 다음 marker가 보여야 한다.

```text
[LAYER-7] ONOFF_TX_REQUEST src=0x0005 dst=0xC000 ...
[LAYER-7] ONOFF_TX_ACCEPTED dst=0xC000 ttl=7 ...
```

세 수신 Node에서는 다음 marker를 확인한다.

```text
[LAYER-7] ONOFF_RX src=0x0005 dst=0xC000 ... state=ON
```

Acknowledged `on` 또는 `off`를 보냈다면 송신 Node에서 각 Server의 Status response도 확인한다.

```text
[LAYER-7] ONOFF_STATUS_RX src=0x0003 ...
[LAYER-7] ONOFF_STATUS_RX src=0x0004 ...
[LAYER-7] ONOFF_STATUS_RX src=0x0005 ...
```

이번 실험에서는 다음 경로를 확인했다.

```text
0x0005 -> group 0xC000 -> 0x0003, 0x0004, 0x0005
0x0003, 0x0004, 0x0005 -> acknowledged Status -> 0x0005
```

이 결과는 three-node group communication PASS다. 세 Node가 가까이 놓인 상태였으므로 Relay를 거쳤다는 증거는 아니다.

## 11. Serial command reference

| 명령 | 동작 |
| --- | --- |
| `on` | Acknowledged Generic OnOff Set ON |
| `off` | Acknowledged Generic OnOff Set OFF |
| `on-unack` | Unacknowledged Generic OnOff Set ON |
| `off-unack` | Unacknowledged Generic OnOff Set OFF |
| `tx-low` | BLE Advertising TX power를 일시적으로 `-24 dBm`으로 낮춤 |
| `tx-normal` | 부팅 때 기록한 정상 TX power로 복구 |
| `status` | Provisioning, key, publication, subscription, Relay, OnOff, TX power 상태 출력 |
| `factory-reset` | Local Mesh state를 삭제하고 재부팅 |

명령은 소문자이며 newline으로 끝나야 한다. 한 줄 buffer는 terminating NUL을 포함해 32 byte다. 너무 긴 명령은 `line-too-long`, 알 수 없는 명령은 `unknown-command`로 거부된다.

`tx-low` 상태는 NVS에 저장되지 않는다. `tx-normal`을 실행하거나 Node를 재부팅하면 정상 출력으로 돌아간다. 이번 보드에서 read-back으로 확인한 값은 low `-24 dBm`, normal `+9 dBm`이다.

현재 A/B의 native USB Serial/JTAG port는 log output은 보이지만 firmware의 `stdin`인 UART0 command input으로 연결되지 않은 구성이었다. D6의 CH343 UART connection에서는 command 입력을 확인했다. 이것은 Mesh Model 역할의 차이가 아니라 serial wiring의 차이다.

## 12. Relay 검증 방법

모든 Node가 같은 firmware에서 Client, Server, Relay capability를 갖지만 Relay는 기본적으로 Disabled다. Relay 실험에서는 물리적으로 가운데 놓은 Node 하나만 Enabled로 바꾼다.

현재 보드 mapping을 사용하는 다음 실험의 권장 topology는 아래와 같다.

```text
iPhone -> 0x0004-ESP32-B6 -> 0x0003-ESP32-76 -> 0x0005-ESP32-D6
          Proxy/near         Relay candidate   Remote receiver
```

정확한 Relay 증명 절차:

1. B6-76과 76-D6는 통신 가능하지만 B6-D6 직접 경로는 닿지 않도록 거리나 차폐를 조절한다.
2. 76의 Relay를 Disabled로 둔다.
3. Source에서 같은 unacknowledged command를 여러 번 보낸다.
4. 76은 수신하지만 D6는 수신하지 않는 기준 상태를 기록한다.
5. 보드 위치와 TX power를 바꾸지 않는다.
6. nRF Mesh에서 76의 Relay만 Enabled로 바꾼다.
7. Source에서 같은 종류의 command를 다시 여러 번 보낸다.
8. D6에서 반복적으로 `ONOFF_RX`가 나타나는지 확인한다.
9. 실험 후 `tx-normal`로 출력 power를 복구한다.

Relay OFF에서 C 미수신, 같은 위치의 Relay ON에서 C 수신을 모두 확인해야 `TRIPLET_RELAY=PASS`라고 기록할 수 있다. `ONOFF_RX`에는 원래 source와 destination은 있지만 바로 직전 Relay의 주소는 표시되지 않으므로, 세 Node가 message를 받았다는 사실만으로 Relay 경로를 증명할 수 없다.

2026-08-28 실내 실험에서는 중간 Relay를 끈 상태에서도 먼 D6의 저장 상태가 `OFF -> ON`으로 바뀌었다. 즉, B6에서 D6로 가는 직접 경로가 남아 있었으므로 장거리 직접 수신까지만 PASS이며 Relay 증거는 아니다. 다음 실험은 더 넓은 장소에서 같은 위치를 유지한 채 다음처럼 진행한다.

1. D6의 시작 상태를 `ON`으로 확인한다.
2. 76 Relay를 Disabled로 두고 group `OFF`를 보낸다.
3. D6가 계속 `ON`인지 확인해 직접 경로 차단을 먼저 증명한다.
4. 위치를 바꾸지 않고 76 Relay만 Enabled로 바꾼다.
5. 같은 group `OFF`를 보내 D6가 `OFF`로 바뀌는지 확인한다.

## 13. Troubleshooting

### AppKey Add 또는 Model Bind가 실패함

표시된 오류:

```text
No GATT Proxy Node is connected or the connected Proxy does not know the
Network Key used to secure this message.
```

해결 순서:

1. `Select Proxy`를 연다.
2. 같은 Mesh network에 Provisioning된 Node를 선택한다.
3. 상태가 `Connected`가 될 때까지 기다린다.
4. 그 Proxy가 현재 network의 NetKey를 사용하는지 확인한다.
5. AppKey Add와 Model Bind를 다시 실행한다.

이번에는 Node-A Proxy 연결 후 Node-A와 Node-B를 구성했고, 전원/연결을 정리한 뒤 Node-C도 구성해 세 Node 모두 같은 AppKey에 Bind했다.

### `Select Proxy`가 `Connecting...`에서 멈춤

- 대상 Node에 전원이 들어오는지 확인한다.
- iPhone과 Node를 가까이 둔다.
- 다른 BLE connection이나 scanner 화면을 닫고 다시 시도한다.
- 꼭 설정 대상 Node에 직접 연결할 필요는 없다. 같은 NetKey를 아는 다른 Proxy Node에 연결해도 된다.

### nRF Mesh scanner에 세 Node가 모두 안 보임

Provisioning 전 scanner와 Provisioning 후 local Node 목록은 다른 화면이다. Provisioning된 Node가 항상 unprovisioned scanner에 다시 나타나는 것은 아니다. 모든 Node가 Mac에 연결될 필요도 없으며, 독립 전원만 있으면 Mesh에 참여한다.

### `TX_REJECTED reason=appkey-not-ready`

Client Model에 AppKey가 Bind되지 않았거나 metadata에 AppKey index가 아직 반영되지 않은 상태다. Node의 Application Key Add와 Generic OnOff Client Bind를 다시 확인한다.

### `TX_REJECTED reason=client-publication-not-configured`

Generic OnOff Client의 Publication Address가 설정되지 않았다. `0xC000`, 같은 AppKey, TTL `7`을 다시 설정한다.

### `No outbound bearer found, inbound bearer 1`

송신 Node 자체도 `0xC000`을 subscribe한 상태에서 stack이 출력할 수 있는 warning이다. 실제 `ONOFF_RX`와 Status response가 정상이라면, 이전 application code가 만들던 `ONOFF_STATUS_PUBLISH_FAILED`와 같은 문제로 해석하지 않는다.

### USB port가 바뀌거나 여러 개 보여 자동 선택 실패

USB를 다시 연결한 뒤 `/dev/cu.*` 목록을 확인하고 `--port`, `--port-a`, `--port-b`, `--port-c`로 정확히 지정한다.

## 14. 현재 검증 상태

| 단계 | 결과 | 증거의 의미 |
| --- | --- | --- |
| Serial parser host test | PASS | 명령, CR/LF, overflow recovery 확인 |
| ESP-IDF v5.5.5 build | PASS | ESP32-S3 firmware image 생성 |
| A/B/C Flash와 Boot | PASS | 세 보드에서 Layer 7 시작, firmware SHA `492c7d279` 기준 |
| 서로 다른 identity | PASS | `76`, `B6`, `D6` 확인 |
| iPhone Provisioning | PASS | 세 Node가 같은 Mesh network에 가입 |
| AppKey Add와 Server/Client Bind | PASS | AppKey index `0x0000` 하나를 전 Node/Model에 사용 |
| Group publication/subscription | PASS | Client publish와 Server subscribe 모두 `0xC000` |
| Three-node one-hop OnOff | PASS | `0x0005` 송신을 세 Node가 수신 |
| Acknowledged Status | PASS | 세 Node의 response를 `0x0005`에서 수신 |
| TX power control | PASS | `-24 dBm`과 `+9 dBm` read-back 확인 |
| Low-power/relocation indoor test | 직접 수신됨 | Relay OFF에서도 D6가 수신해 직접 경로가 남아 있었음 |
| Controlled Relay OFF/ON comparison | DEFERRED / NOT_VERIFIED | 더 넓은 장소에서 동일 위치 OFF 미수신, ON 수신 대조가 남음 |

이 표에서 Build PASS는 보드 실행을 뜻하지 않고, Boot PASS는 Provisioning을 뜻하지 않으며, Provisioning PASS는 AppKey/Model Configuration이나 Relay PASS를 뜻하지 않는다. 각 단계를 별도로 확인한다.

## 15. 관련 파일

- [Layer 7 사용법과 최신 상태](../layers/layer-7/README.md)
- [Layer 7 firmware entry point](../../code/layers/layer-7/main/main.c)
- [Mesh Node 구현](../../code/layers/layer-7/main/mesh_node.c)
- [Serial command parser](../../code/layers/layer-7/main/serial_command.c)
- [한 보드 Build/Flash/Boot workflow](../../code/layers/layer-7/bootload.sh)
- [세 보드 Build/Flash/Boot workflow](../../code/layers/layer-7/bootload-triplet.sh)
- [세 보드 monitor](../../code/layers/layer-7/monitor-triplet.sh)
- [BLE Mesh 용어](../03-reference/TERMS.md)
- [이전 Layer별 검증 기록](../01-project/STATUS.md)
