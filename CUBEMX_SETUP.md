# CubeMX 프로젝트 시작 방법

팀원 한 명이 NUCLEO-F411RE 보드 한 개와 자기 모듈을 맡는다. 세 STM32 프로젝트는 같은 보드를 사용하지만 센서와 핀 설정이 다르므로 각각 따로 생성한다.

## 생성 위치

| 담당 | CubeMX 프로젝트 위치 | 권장 프로젝트 이름 |
|---|---|---|
| 팀원 1 | `modules/01-sensor-module/stm32/` | `module01_sensor` |
| 팀원 2 | `modules/02-sensor-module/stm32/` | `module02_sensor` |
| 팀원 3 | `modules/03-communication/stm32/` | `module03_communication` |

CubeMX가 프로젝트 이름으로 하위 폴더를 한 번 더 만들도록 설정되어 있다면 다음 구조도 괜찮다.

```text
modules/01-sensor-module/stm32/module01_sensor/
modules/02-sensor-module/stm32/module02_sensor/
modules/03-communication/stm32/module03_communication/
```

중요한 것은 각 프로젝트가 자기 모듈 폴더 밖에 파일을 생성하지 않고, 다른 팀원의 CubeMX 프로젝트와 섞이지 않는 것이다.

## 생성하기 전에 팀이 먼저 정할 것

다음 항목은 세 사람이 공통으로 결정한 뒤 CubeMX를 설정한다.

- STM32와 ESP32-S3 사이에 사용할 UART 장치
- UART TX/RX 핀
- UART 속도와 데이터 형식
- 노드 번호 규칙
- 공통 메시지 형식

첫 시험의 UART 기본값은 다음을 권장한다.

```text
Baud rate: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
Logic level: 3.3V
필수 연결: TX + RX + GND
```

NUCLEO-F411RE의 USART2 PA2/PA3는 ST-LINK Virtual COM Port를 통한 PC 로그에 사용할 수 있다. USART2를 PC 로그용으로 유지하려면 ESP32-S3 통신에는 USART1 또는 USART6 같은 다른 UART 후보를 검토한다. 최종 UART와 핀은 각 센서 핀과 겹치지 않는지 CubeMX에서 확인한 뒤 공통으로 확정한다.

## CubeMX 생성 순서

1. STM32CubeMX를 실행한다.
2. `Board Selector`에서 `NUCLEO-F411RE`를 선택한다.
3. 필요한 센서의 GPIO, ADC, I2C, Timer 등을 설정한다.
4. 팀에서 정한 ESP32-S3 통신용 UART를 설정한다.
5. 필요하면 USART2를 PC 로그용으로 설정한다.
6. `Project Manager`에서 자기 모듈의 고유한 프로젝트 이름을 입력한다.
7. 프로젝트 생성 위치가 자기 `stm32/` 폴더 안인지 확인한다.
8. Toolchain 또는 빌드 형식으로 `CMake`를 선택한다.
9. 코드를 생성한다.
10. 생성 직후 아무 기능을 추가하지 않은 상태에서 먼저 빌드한다.

## 생성 후 예상 파일

CubeMX 버전과 설정에 따라 세부 위치는 달라질 수 있지만 다음 파일은 Git에 포함한다.

```text
Core/
Drivers/
cmake/
CMakeLists.txt
CMakePresets.json
module-name.ioc
STM32F411xx_FLASH.ld
startup_stm32f411xe.s
```

다음 빌드 결과물은 Git에 올리지 않는다.

```text
build/
Debug/
Release/
*.elf
*.hex
*.bin
*.map
```

저장소의 `.gitignore`에 위 결과물이 등록되어 있다.

## 코드 위치 원칙

- CubeMX가 생성한 HAL과 보드 코드는 각 프로젝트의 `Core/`, `Drivers/`에 둔다.
- 특정 센서 드라이버는 해당 모듈의 STM32 프로젝트 안에서 관리한다.
- UART 패킷, 메시지 파싱, 노드 설정처럼 세 모듈이 함께 사용할 코드는 `common/stm32/`에서 관리한다.
- CubeMX가 다시 생성할 수 있는 파일에 직접 코드를 작성할 때는 `USER CODE BEGIN`과 `USER CODE END` 영역을 사용한다.
- `03-communication/esp32-s3`와 `03-communication/pico2`는 CubeMX 프로젝트가 아니므로 각각 ESP-IDF와 Pico SDK를 사용한다.

## 첫 커밋 전 확인

- [ ] 보드가 `NUCLEO-F411RE`인지 확인
- [ ] MCU가 `STM32F411RETx`인지 확인
- [ ] 프로젝트가 자기 모듈의 `stm32/` 안에 생성됐는지 확인
- [ ] `.ioc`, `Core`, `Drivers`와 CMake 파일이 있는지 확인
- [ ] UART와 핀이 팀의 공통 결정과 일치하는지 확인
- [ ] 생성 직후 CMake 빌드가 성공하는지 확인
- [ ] `build`, `.elf`, `.hex`, `.bin`이 Git에 포함되지 않는지 확인

## 브랜치 이름

각 팀원은 자기 모듈 브랜치에서 작업한다.

```text
module/01-sensor
module/02-sensor
module/03-communication
```

첫 커밋에는 센서 기능을 한꺼번에 넣기보다 CubeMX 생성 파일과 빌드 가능한 기본 상태를 먼저 올린다. 그다음 센서 드라이버와 모듈 기능을 작은 단위로 추가한다.
