# NOSTOS Firmware

STM32F411RE가 버튼·센서 입력과 LED·부저·음성 출력을 처리하고, ESP32-S3가 Bluetooth Mesh로 이벤트를 전달합니다.

이 디렉터리는 버전별 복사본이 아니라 **현재 활성 소스 한 벌**입니다. 현재 기본 wire protocol은 v1이고 선택형 v2 호환 경로는 기본 설정에서 비활성화되어 있습니다. 펌웨어 bundle 버전과 wire protocol 버전은 서로 독립적으로 관리합니다. 전체 정책은 [구조 기준 문서](../STRUCTURE.md)를 참고합니다.

```text
firmware/
├── stm32/                 NUCLEO-F411RE 애플리케이션·HAL·오디오 자산
├── esp32/                 ESP32-S3 UART1 ↔ Bluetooth Mesh bridge
├── protocol/              양쪽 펌웨어가 함께 빌드하는 메시지 계약
├── profiles/              추적하는 빌드·패키지 정책
├── inventory/             장비 inventory 예시; 실제 값은 로컬 전용
├── tools/fw               통합 검사·빌드·패키지·검증·Flash plan 진입점
├── out/                   무시되는 빌드·릴리스 패키지
├── build.sh               STM32/ESP32 빌드 진입점
├── test-host.sh           보드 없는 전체 회귀 검사
├── flash-stm32-all.sh     과거 STM32 배포 검증용 legacy 도구
└── VERSION                현재 bundle 버전 원본
```

## 기본 동작

- BTN1: SPEED UP(`0x11`), 초록 RGB + 로컬 오디오 + UART 전송
- BTN2: SPEED DOWN(`0x10`), 노랑 RGB + 로컬 오디오 + UART 전송
- BTN3: STOP(`0x13`), 빨강 RGB + 로컬 오디오 + UART 전송
- 상대 BTN1~3 수신: 상대 STM32에서 오디오만 재생
- BTN1 → BTN2 → BTN3 → BTN4: MPU6050 장착 자세 캘리브레이션
- SSD1306: I2C1 PB8/PB9, DHT11: PA1
- 부저: 확정 `FALL_DETECTED`에서만 동작

STM32는 `NOSTOS_PROTOCOL_V2=OFF`, ESP32는 `CONFIG_NOSTOS_PROTOCOL_V2=n`이 기본입니다.

## UART 배선

| 방향 | STM32F411RE | ESP32-S3 |
| --- | --- | --- |
| STM32 → ESP32 | USART1 TX, D8(PA9) | GPIO18, UART1 RX |
| ESP32 → STM32 | USART1 RX, D2(PA10) | GPIO17, UART1 TX |
| 공통 | GND | GND |

양쪽 모두 115200 baud, 8N1입니다.

## 검사와 빌드

보드 없이 STM32·ESP32·protocol 호스트 검사를 실행합니다.

```sh
bash firmware/tools/fw test
```

STM32 Release를 빌드합니다.

```sh
bash firmware/tools/fw build stm32
```

ESP-IDF v5.5.5 환경을 활성화한 뒤 ESP32-S3를 빌드합니다.

```sh
bash firmware/tools/fw build esp32
```

통합 도구의 `test`와 `build`는 Flash·erase·reset·provisioning을 수행하지 않습니다. `flash`도 build를 호출하지 않으며, 현재 단계에서는 release package를 대상으로 한 dry-run 계획만 제공합니다. legacy STM32 도구의 실제 실행은 별도 승인 대상입니다.

## 현재 v1 baseline

2026-08-29 기준 기록은 다음과 같습니다. 루트 이동 후 현재 경로에서 다시 빌드한 이미지와 기존 실물 배포 이미지를 구분해 기록합니다.

- host-tests: STM32 Debug/Release/Sanitized 각각 9/9, ESP32 각각 4/4, protocol Sanitized 2/2(전체 41/41)
- 현재 STM32 Release: 137,632 B, RAM 3,936 B, SHA-256 `d76589510f81d40f09ac5a31a373481dca448e919d495a7a267f6620eaaf91b0` (Ninja release profile 빌드, 미설치)
- 현재 ESP32-S3 app: 915,856 B, partition 여유 620,144 B, SHA-256 `1b27654a14b409678c59b51958727789c94e38071caa07e7147eafd7345684ea` (`PROJECT_VER=v1`, ESP-IDF v5.5.5, 미설치, 빌드 시각 포함으로 재현 가능한 해시는 아님)
- 기존 STM32 3대 배포 이미지: 137,632 B, SHA-256 `d76589510f81d40f09ac5a31a373481dca448e919d495a7a267f6620eaaf91b0`; write/read-back byte 일치와 reset 확인
- ESP32: 현재 루트 이미지는 빌드만 확인했으며 Flash하지 않음

STM32의 오브젝트 내용은 같아도 CMake generator의 링크 순서에 따라 최종 BIN 해시가 달라질 수 있습니다. release profile은 Ninja를 고정하며 `flash-stm32-all.sh`는 실제 3대에서 read-back 확인된 기존 배포 해시만 허용합니다. 이번 재빌드 산출물은 기존 허용 hash와 같지만 Flash하지 않았습니다.

위 수치는 정식 `v1.0.0` release가 아니라 구조 변경 전 baseline입니다. 원본 기록은 [2026-08-29 v1 baseline](../releases/baselines/2026-08-29-v1.json)에 보존합니다. 정확한 재배포가 필요한 정식 release는 Git에서 제외되는 `firmware/out/releases/`에 실제 BIN과 SHA manifest를 함께 보존해야 합니다.

빌드와 read-back 일치는 버튼·센서·오디오·무선 다중 홉의 실물 동작을 대신하지 않습니다. Mesh 주소·NetKey·AppKey·Publication·Subscription은 각 ESP32의 NVS에 별도로 존재하며 이 저장소에 포함하지 않습니다.

세부 사용법은 [STM32](stm32/README.md), [ESP32-S3](esp32/README.md), [protocol](protocol/README.md)을 참고합니다.
