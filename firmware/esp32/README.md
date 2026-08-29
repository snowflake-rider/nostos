# NOSTOS Firmware — ESP32-S3

ESP32-S3용 UART1 ↔ Bluetooth Mesh bridge입니다. ESP-IDF v5.5.5를 기준으로 하며 `CONFIG_NOSTOS_PROTOCOL_V2=n`이 기본입니다. 공통 코드는 `../protocol/`을 직접 빌드합니다.

## 연결

- GPIO18, UART1 RX ← STM32 PA9/D8, USART1 TX
- GPIO17, UART1 TX → STM32 PA10/D2, USART1 RX
- 115200 baud, 8N1, 공통 GND

## 빌드와 호스트 검사

ESP-IDF 환경에서 다음을 실행합니다.

```sh
cd firmware/esp32
bash test-host.sh
idf.py build
```

또는 저장소 루트에서:

```sh
bash firmware/tools/fw build esp32
```

타깃은 `esp32s3`이며 `sdkconfig`, `sdkconfig.defaults`, `sdkconfig.defaults.esp32s3`를 버전 관리합니다. build 산출물과 `dependencies.lock`은 생성 파일입니다.

## 현재 기본 전달 경로

```text
STM32 UART RX → event bridge queue → Bluetooth Mesh TX
Bluetooth Mesh RX → event bridge queue → STM32 UART TX
```

기본 v1 payload는 메시지 ID 앞에 버전 바이트를 붙인 2바이트 형식입니다. 실제 수신 완료는 송신 로그만으로 판단하지 않고 상대 ESP32의 Mesh RX, UART TX와 상대 STM32의 출력까지 확인합니다.

## Mesh 설정 경계

Mesh 주소, NetKey, AppKey, Publication, Subscription은 각 보드 NVS의 provisioning 상태입니다. Git에는 키, NVS/Flash 백업, 장치별 provisioning export를 포함하지 않습니다.

빌드 또는 Flash만으로 새 보드가 기존 Mesh 네트워크에 참여하지 않습니다. 이 저장소 정리에서는 Flash·erase·reset·provisioning·Mesh 키 변경을 수행하지 않습니다.

## Flash 경계

통합 도구의 ESP32 Flash는 현재 실제 명령을 실행하지 않고 검증된 계획만 출력합니다. 기본 계획은
application만 `0x10000`에 쓰고 partition table과 NVS를 보존합니다. 따라서 로컬
`inventory/boards.local.json`에 기록한 장비의 partition layout ID와 실제 확인한 partition-table
SHA-256이 release package와 모두 일치해야 dry-run 계획이 생성됩니다.

```sh
bash firmware/tools/fw flash --release nostos-v1.0.0 --target esp32 --node rider-1 --dry-run
```

layout이나 hash가 다르면 application-only Flash를 진행하지 말고 별도 migration 또는 reprovision
절차를 먼저 정의해야 합니다.
