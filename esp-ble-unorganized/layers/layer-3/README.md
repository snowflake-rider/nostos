# ESP32-S3 Layer 3: 가장 작은 BLE Active Scanner

## 목표

두 ESP32-S3 사이의 실제 connectionless BLE 수신을 확인한다.

```text
Board A: Layer 2 ESP32-LAYER-2 Advertising
                    ↓ BLE radio
Board B: Layer 3 Active Scanner
                    ↓
         name + Service UUID + RSSI + count
```

Layer 3는 Board A에 연결하지 않는다. Advertising packet과 scan response를
받는 Observer/Scanner 역할만 한다.

## 보드 배치

```text
Board A: 독립 USB 전원, Layer 2, phone과 disconnect
Board B: Mac USB data 연결, Layer 3 flash 대상
Board C: 이번 Layer에서는 unplugged
```

Board A가 phone과 GATT 연결된 상태면 Advertising이 멈추므로 Layer 3가
찾을 수 없다.

## Active Scan을 쓰는 이유

Layer 2는 packet을 다음처럼 나눈다.

```text
Primary Advertising: 128-bit Service UUID
Scan response:        ESP32-LAYER-2 이름
```

그래서 Layer 3는 scan request를 보내 scan response까지 받는 Active Scan을
사용한다. 이름과 UUID가 같은 result에 모두 있어야 target으로 인정한다.

## 한 번에 Bootload

Board A가 켜져 있고 phone과 연결되지 않은 상태에서 실행한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-3
./bootload.sh
```

port를 직접 지정하려면:

```bash
./bootload.sh --port /dev/cu.usbmodem1401
```

## 자동 PASS 기준

```text
Board B ESP32-S3 + 16 MB Flash profile
bootloader + partition table + application build
Flash image hash verification
Board B app_main() 진입
BLE Controller 및 Bluedroid 활성화
Active Scanning 시작
Board A name + Service UUID match
[LAYER-3] TARGET_RX 수신
```

`SCAN_STARTED`만으로는 PASS가 아니다. Board A의 실제 packet을 받아야
`RESULT=PASS`가 된다.

## 예상 Serial Marker

```text
[LAYER-3] BOOT_SUCCESS
[LAYER-3] SCAN_PARAMS_READY
[LAYER-3] SCANNING_STARTED mode=active
[LAYER-3] TARGET_FOUND name=ESP32-LAYER-2 rssi=-45
[LAYER-3] SERVICE_MATCH uuid=7A110000-6B0D-4D5A-8F4B-2C9E00000001
[LAYER-3] TARGET_RX count=1 rssi=-45
[LAYER-3] SCAN_TARGET_CONFIRMED
[LAYER-3] SCANNER_ACTIVE target_count=...
```

첫 target packet과 이후 매 10번째 packet만 `TARGET_RX`로 출력한다. 주변의
모든 BLE packet을 출력하지 않아 serial log가 불필요하게 커지지 않는다.

## Layer 3에서 증명하는 것

```text
Source/build/Flash/boot      자동 검증
BLE Active Scanner 시작     자동 검증
Board A radio transmission  실제 Board B 수신으로 검증
Name/Service UUID/RSSI      실제 packet parsing으로 검증
```

## 의도적으로 제외한 기능

- Board B Advertising
- GATT Server 또는 GATT Client
- Board A와 connection
- GATT Read/Write/Notify
- Bluetooth Mesh와 Provisioning
- relay, TTL, deduplication, retransmission

## NVS 주의

Layer 3는 `erase-flash`와 `nvs_flash_erase()`를 실행하지 않는다. NVS
초기화가 실패하면 기존 데이터를 자동 삭제하지 않고 workflow를 실패 처리한다.
