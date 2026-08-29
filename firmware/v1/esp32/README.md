# NOSTOS Firmware v1 — ESP32-S3

`firmware/esp32/`의 기본 Layer 8 UART↔Bluetooth Mesh bridge를 v1 묶음으로 고정한 소스입니다. 타깃은 ESP32-S3이고 ESP-IDF v5.5.5를 기준으로 합니다. `CONFIG_NOSTOS_PROTOCOL_V2` 기본값은 `n`이며, STM32와 동일하게 1바이트 v1 메시지를 사용합니다.

## 연결

- ESP32 GPIO18(UART1 RX) ← STM32 PA9/D8(USART1 TX)
- ESP32 GPIO17(UART1 TX) → STM32 PA10/D2(USART1 RX)
- 115200 baud, 8N1, 공통 GND

## 빌드와 호스트 검사

```sh
cd firmware/v1/esp32
bash test-host.sh
idf.py build
```

`idf.py` 실행 전 ESP-IDF v5.5.5 환경을 활성화해야 합니다. 공통 코드는 같은 묶음의 `../protocol/`을 직접 사용합니다.

## Mesh 설정 경계

Mesh 주소, NetKey, AppKey, Publication, Subscription은 보드 NVS의 provisioning 상태입니다. 이 Git 디렉터리에는 키, NVS/Flash 백업, Mesh export가 포함되지 않습니다. 소스를 빌드하거나 플래시하는 것만으로 기존 네트워크 설정이 새 보드에 복제되지는 않습니다.

기본 v1 코드는 UART에서 BTN1/2/3 메시지를 받아 Mesh로 보내고, 상대 Mesh 메시지를 UART로 STM32에 전달합니다. 실제 수신 완료는 `UART_RX` → `MESH_TX` → 상대 `MESH_RX` → `UART_TX` 로그와 상대 STM32의 오디오로 별도 확인합니다.
