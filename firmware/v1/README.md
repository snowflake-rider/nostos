# NOSTOS Firmware v1

2026-08-29에 실물 확인한 STM32F411RE + ESP32-S3 Prototype 조합을 한 디렉터리에 묶은 v1 소스입니다.

```text
firmware/v1/
├── stm32/      NUCLEO-F411RE 애플리케이션·HAL·오디오 자산
├── esp32/      ESP32-S3 UART1 ↔ Bluetooth Mesh bridge
├── protocol/   두 펌웨어가 함께 빌드하는 메시지 계약
├── VERSION
├── manifest.json
├── test-host.sh
└── build.sh
```

## 기본 동작

- SSD1306: I2C1 PB8/PB9에서 DHT11 온·습도와 `DHT WAIT/OK/ERROR` 표시
- DHT11: PA1에서 약 1.2초마다 로컬 측정
- SSD1306·DHT11은 기본 ON이며 `BUTTON_OUTPUT_TEST`에서는 자동 OFF
- BTN1: SPEED UP(`0x11`), 초록 RGB + 로컬 오디오 + UART 전송
- BTN2: SPEED DOWN(`0x10`), 노랑 RGB + 로컬 오디오 + UART 전송
- BTN3: STOP(`0x13`), 빨강 RGB + 로컬 오디오 + UART 전송
- 상대 BTN1~3 수신: 상대 STM32 오디오만 재생
- BTN1 → BTN2 → BTN3 → BTN4: MPU6050 장착 자세 캘리브레이션
- 캘리브레이션 완료: `calibration_completed.mp3` 한 번 재생
- 초기 RGB OFF, 캘리브레이션 성공 후 `REAR_SAFE`에서 초록 점등
- 부저는 확정 `FALL_DETECTED`에서만 울림

기본 구성은 양쪽 모두 v1입니다. STM32는 `NOSTOS_PROTOCOL_V2=OFF`, `BUTTON_OUTPUT_TEST=OFF`; ESP32는 `CONFIG_NOSTOS_PROTOCOL_V2=n`입니다.

## UART 배선

| 방향 | STM32F411RE | ESP32-S3 |
| --- | --- | --- |
| STM32 → ESP32 | USART1 TX, D8(PA9) | GPIO18, UART1 RX |
| ESP32 → STM32 | USART1 RX, D2(PA10) | GPIO17, UART1 TX |
| 공통 | GND | GND |

양쪽 모두 115200 baud, 8 data bits, no parity, 1 stop bit입니다.

## 검사와 빌드

보드 없이 세 묶음을 검사합니다.

```sh
bash firmware/v1/test-host.sh
```

STM32 Release만 빌드합니다.

```sh
bash firmware/v1/build.sh stm32
```

연결된 STM32F411 세 대에 검증된 v1 이미지를 병렬 Flash하고 read-back까지 확인합니다.
일반 macOS 터미널에서 실행하면 관리자 암호를 한 번 요청합니다.

```sh
bash firmware/v1/flash-stm32-all.sh
```

실제 write 없이 이미지와 대상만 확인하려면 `--dry-run`을 붙입니다. 이 스크립트는
전체 chip erase, option byte/OTP, ESP32 NVS·Mesh 설정을 변경하지 않습니다.
write를 다시 하지 않고 현재 Flash 내용만 read-back 검증·reset하려면 `--verify-only`를 붙입니다.

ESP-IDF v5.5.5 환경을 활성화한 뒤 ESP32-S3를 빌드합니다.

```sh
bash firmware/v1/build.sh esp32
```

이 스크립트들은 Flash, erase, reset, provisioning을 수행하지 않습니다.
컴파일 산출물은 저장소 정책에 따라 Git에 넣지 않습니다. 현재 v1 소스·설정·빌드 명령과 정확한 결과 해시는 `manifest.json`에 고정되어 있으며 같은 명령으로 다시 생성합니다.

### 2026-08-29 v1 빌드 체크포인트

- v1 host-tests: STM32 Debug/Release/Sanitized 각각 9/9, ESP32 각각 4/4, protocol 3/3 통과
- STM32 Release: SSD1306/DHT11 ON, Flash 137,632 B, RAM 3,936 B, SHA-256 `d76589510f81d40f09ac5a31a373481dca448e919d495a7a267f6620eaaf91b0`
- ESP32-S3 Release: ESP-IDF v5.5.5, app 915,856 B, partition 여유 620,144 B, SHA-256 `36b9b2cee94e87db863ae07d636e71b254d8cfb253b7b75b05f1f2ab6cb9eaf8`

## 실물 확인 범위

2026-08-29 사용자가 두 조합에서 BTN1/2/3의 로컬 오디오와 상대 보드 오디오 재생을 확인했습니다. 이 기록은 버튼 UART/Mesh 전달 경로의 수동 확인이며, 장기 무선 안정성이나 실제 낙상 판정을 증명하지 않습니다.

같은 날 STM32F411 세 대에 화면 이식 전 이미지 134,980바이트를 먼저 플래시한 뒤, 화면·DHT11 포함 현재 v1 이미지 137,632바이트를 세 대에 병렬 플래시했습니다. ST-LINK 시리얼 `066DFF485277504867161930`, `066DFF505567494867071811`, `066EFF3134584B3043121635`의 개별 read-back은 모두 SHA-256 `d76589510f81d40f09ac5a31a373481dca448e919d495a7a267f6620eaaf91b0` 및 byte-for-byte 비교가 일치했고 reset까지 성공했습니다. Flash byte 일치는 버튼·센서·오디오의 재부팅 후 실물 동작을 대신하지 않습니다.

ESP32 Mesh 주소·NetKey·AppKey·Publication·Subscription은 각 보드 NVS에 따로 존재합니다. 이 디렉터리에는 Mesh 키, NVS/Flash 백업, 장치별 provisioning export를 포함하지 않습니다. 새 보드는 소스 Flash와 별도로 같은 Mesh 네트워크에 안전하게 provisioning해야 합니다.
