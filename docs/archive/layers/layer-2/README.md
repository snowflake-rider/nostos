> 이관 원문: `layers/layer-2/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 2: 가장 작은 Connectable GATT Server

## 목표

Layer 1의 전체 bootloading 및 BLE 초기화 흐름을 유지하면서 스마트폰이
연결할 수 있는 GATT Server를 만든다.

```text
Phone --RX Write "HELLO"--> ESP32-LAYER-2
Phone <--TX Notify "ACK:HELLO"-- ESP32-LAYER-2
Phone <--TX Read "ACK:HELLO"--- ESP32-LAYER-2
```

## 이 Layer가 하는 일

```text
Layer 1 device/build/Flash 검증
-> NVS, BLE Controller, Bluedroid 초기화
-> 128-bit custom GATT Service 생성
-> RX Write Characteristic 생성
-> TX Read/Notify Characteristic와 CCCD 생성
-> connectable Advertising 시작
-> 스마트폰 연결과 HELLO/ACK 교환
-> 연결 해제 후 Advertising 자동 재시작
```

이것은 일반 BLE GATT 연결 학습 단계다. Bluetooth Mesh나 relay가 아니다.

## BLE Interface

| 항목 | UUID | 기능 |
| --- | --- | --- |
| Service | `7a110000-6b0d-4d5a-8f4b-2c9e00000001` | RX/TX 묶음 |
| RX | `7a110000-6b0d-4d5a-8f4b-2c9e00000002` | Phone -> ESP32 Write |
| TX | `7a110000-6b0d-4d5a-8f4b-2c9e00000003` | ESP32 -> Phone Read/Notify |

RX payload는 1~20 bytes다. TX response도 최대 20 bytes이며 긴 입력은
`ACK:` 뒤에서 안전하게 잘린다.

## 한 번에 Bootload

ESP32-S3 한 대만 USB로 연결한 뒤 실행한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-2
./bootload.sh
```

여러 serial 장치가 보이면 port를 지정한다.

```bash
./bootload.sh --port /dev/cu.usbmodem5C4C2165221
```

## 자동 PASS 기준

```text
ESP32-S3 + 16 MB Flash profile
bootloader + partition table + application build
Flash image hash verification
app_main() 진입
BLE Controller 및 Bluedroid 활성화
GATT Service/RX/TX/CCCD 생성
connectable Advertising 시작
[LAYER-2] GATT_SERVER_ACTIVE 수신
```

자동 PASS는 firmware가 새 보드에서 실행되고 GATT Server가 준비됐다는
증거다. 휴대폰 Connect/Write/Read/Notify 증거는 다음 단계로 분리한다.

## nRF Connect에서 실제 GATT 확인

1. `Scanner`에서 `ESP32-LAYER-2`를 찾는다.
2. `Connect`를 누른다.
3. 위 표의 custom Service를 연다.
4. TX Characteristic의 Notification을 활성화한다.
5. RX Characteristic에서 UTF-8/Text `HELLO`를 Write한다.
6. TX Notification으로 `ACK:HELLO`가 오는지 확인한다.
7. TX를 Read하여 같은 `ACK:HELLO`가 나오는지 확인한다.
8. Disconnect한 뒤 scanner에서 다시 Advertising되는지 확인한다.

HEX mode를 사용한다면 `HELLO`는 다음 bytes다.

```text
48 45 4C 4C 4F
```

예상 ACK bytes는 다음과 같다.

```text
41 43 4B 3A 48 45 4C 4C 4F
```

폰 검증 전에는 bootload log의 `PHONE_GATT_TEST=NOT_VERIFIED`가 정상이다.

## 핵심 Serial Marker

```text
[LAYER-2] GATT_SERVICE_READY
[LAYER-2] ADVERTISING_STARTED name=ESP32-LAYER-2 type=connectable
[LAYER-2] GATT_SERVER_READY
[LAYER-2] CONNECTED
[LAYER-2] NOTIFY_ENABLED
[LAYER-2] RX_WRITE len=5 value=HELLO
[LAYER-2] TX_NOTIFY len=9 value=ACK:HELLO
[LAYER-2] TX_READ len=9 value=ACK:HELLO
[LAYER-2] DISCONNECTED
[LAYER-2] ADVERTISING_RESTARTED
```

## 의도적으로 제외한 기능

- ESP32 Scanning 및 GATT Client
- Pairing/Bonding/Security
- 여러 장치 동시 연결
- Bluetooth Mesh와 Provisioning
- multi-node Relay, TTL, deduplication
- production packet framing, CRC, persistence, OTA

## NVS 주의

Layer 2는 `erase-flash`와 `nvs_flash_erase()`를 실행하지 않는다. NVS
초기화가 실패하면 기존 데이터를 자동 삭제하지 않고 workflow를 실패 처리한다.
