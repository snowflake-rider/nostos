> 이관 원문: `layers/layer-1/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 1: 가장 작은 BLE Advertising

## 목표

Layer 0의 전체 bootloading workflow를 유지하면서 BLE Controller와 Bluedroid Host를 초기화하고, 연결을 받지 않는 이름 Advertising을 시작한다.

```text
ESP32-LAYER-1
```

## 이 Layer가 하는 일

```text
Layer 0 device/build/Flash 검증
-> NVS 초기화
-> BLE Controller 활성화
-> Bluedroid Host 활성화
-> GAP callback 등록
-> 이름이 들어간 Advertising data 설정
-> non-connectable Advertising 시작
-> serial ADVERTISING_ACTIVE 반복 확인
```

`ADV_TYPE_NONCONN_IND`이므로 스마트폰에서 검색할 수 있지만 연결, GATT service, read/write/notify는 제공하지 않는다. Bluetooth Mesh도 아니다.

## 한 번에 실행

ESP32-S3 한 대만 USB로 연결한 뒤 실행한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-1
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
BLE Controller 활성화
Bluedroid Host 활성화
Advertising start callback 성공
[LAYER-1] ADVERTISING_ACTIVE 수신
```

자동 PASS는 ESP-IDF stack이 Advertising 시작 성공을 보고했다는 증거다. 실제 공중파 수신 증거는 아래 스마트폰 scan으로 분리한다.

## 스마트폰에서 실제 BLE packet 확인

1. nRF Connect를 연다.
2. `Scanner`에서 `Scan`을 누른다.
3. `ESP32-LAYER-1`을 찾는다.
4. 이름과 RSSI가 갱신되는지 확인한다.
5. 이 Layer는 non-connectable이므로 Connect를 시도하지 않는다.

스마트폰 scan을 확인하기 전에는 `OVER_AIR_SCAN=NOT_VERIFIED` 상태다.

## Source provenance

- 프로젝트 작성: `main/main.c`의 단계별 checkpoint, 최소 Advertising 구성, one-stop 검증 script
- ESP-IDF API: NVS, Bluetooth Controller, Bluedroid, GAP callback과 Advertising API
- 참고 기준: 설치된 ESP-IDF v5.5.5 공식 Bluedroid BLE sender 예제의 초기화와 GAP event 순서

## 의도적으로 제외한 기능

- BLE Scanning
- Connectable Advertising
- GATT Server/Client
- Service/Characteristic
- Pairing/Security
- Bluetooth Mesh/Provisioning/Relay
- 사용자 packet protocol

## NVS 주의

Layer 1은 `erase-flash`와 `nvs_flash_erase()`를 실행하지 않는다. NVS 초기화가 실패하면 기존 데이터를 자동 삭제하지 않고 workflow를 실패 처리한다.
