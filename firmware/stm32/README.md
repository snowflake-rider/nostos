# NOSTOS Firmware — STM32F411RE

NUCLEO-F411RE용 활성 애플리케이션입니다. `Core/`, `Drivers/`, `MyApp/`의 STM32CubeMX 경계를 유지하며 공통 메시지 계약은 `../protocol/`을 직접 빌드합니다. 현재 기본 protocol 설정은 v1입니다.

## 빌드와 검사

Arm GNU Toolchain과 CMake가 필요합니다. Ninja가 없으면 `firmware/build.sh`가 Unix Makefiles를 사용합니다.

```sh
cd firmware/stm32
cmake --preset Debug
cmake --build --preset Debug
```

저장소 루트에서는 다음 명령을 사용합니다.

```sh
bash firmware/stm32/tools/test-host.sh
bash firmware/tools/fw build stm32
```

기본 Release 설정은 `NOSTOS_PROTOCOL_V2=OFF`, `BUTTON_OUTPUT_TEST=OFF`, `SSD1306_DISPLAY=ON`, `DHT11_SENSOR=ON`입니다. CubeMX 입력은 [nostos_stm32.ioc](nostos_stm32.ioc)입니다.

## 현재 기본 동작

- BTN1: `SPEED UP`(0x11), 초록 RGB + 로컬 오디오 + USART1 TX
- BTN2: `SPEED DOWN`(0x10), 노랑 RGB + 로컬 오디오 + USART1 TX
- BTN3: `STOP`(0x13), 빨강 RGB + 로컬 오디오 + USART1 TX
- USART1 RX로 받은 BTN1~3: 원격 오디오만 재생
- BTN4 단독: 메시지·RGB·오디오·부저 없음
- BTN1 → BTN2 → BTN3 → BTN4를 5초 안에 입력: MPU6050 캘리브레이션
- 부저: 확정 `FALL_DETECTED`에서만 동작

캘리브레이션은 50ms 간격의 안정된 샘플 40개를 수집하며 메인 루프와 UART를 막는 `HAL_Delay()`를 사용하지 않습니다. 기준값은 RAM에 있으므로 재부팅 후 다시 캘리브레이션해야 합니다.

## 화면과 센서

| 장치 | 연결 | 동작 |
| --- | --- | --- |
| SSD1306 | I2C1 PB8/PB9, 기본 주소 0x3C | `DHT WAIT/OK/ERROR`, 온도·습도 표시 |
| DHT11 | PA1 | 약 1.2초마다 측정 |
| MPU6050 | I2C1 | 장착 자세 캘리브레이션·낙상 판정 |
| VS1003B | SPI | 로컬/원격 안내 음원 재생 |

MPU6050과 OLED는 같은 I2C1을 순차 사용합니다. DHT11 측정 구간의 IRQ 제한, OLED 주소·풀업, 실제 UART 부하에서의 수신 안정성은 실물 검증 대상입니다.

## Flash 경계

`bash firmware/tools/fw build stm32`는 `firmware/stm32/build/Release/nostos_stm32.bin`까지만 생성합니다. Flash하지 않습니다.

통합 Flash 경계는 release package와 local inventory입니다. 현재 `fw flash`는 실제 장비 명령을 실행하지 않고 계획만 보여줍니다.

```sh
bash firmware/tools/fw flash --release nostos-v1.0.0 --target stm32 --node rider-1 --dry-run
```

`firmware/flash-stm32-all.sh`는 2026-08-29 배포 이미지와 세 ST-LINK를 검증하기 위해 남긴 legacy 도구입니다. 장비 serial은 Git에 두지 않으며 `firmware/inventory/boards.local.json`에서 활성화된 STM32/ST-LINK 장비 3대를 실행 시점에 읽습니다. 새 release 흐름의 일반 진입점이 아닙니다. 실제 실행은 보드 대상과 보존할 상태를 확인하고 승인한 경우에만 수행합니다. 전체 chip erase, option byte, OTP는 범위가 아닙니다.

빌드·Flash byte 일치는 버튼, RGB, VS1003B, MPU6050, SSD1306, DHT11, USART1과 Mesh의 실물 동작을 대신하지 않습니다.
