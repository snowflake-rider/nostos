# CubeMX 프로젝트 시작 방법

팀원 한 명이 NUCLEO-F411RE 보드 한 개와 자기 모듈을 맡는다. 세 STM32 프로젝트는 같은 보드를 사용하지만 센서와 핀 설정이 다르므로 각각 따로 생성한다.

## 정확한 보드와 MCU 이름

CubeMX 상단에 다음과 같이 표시되는지 확인한다.

```text
STM32F411RETx - NUCLEO-F411RE
```

두 이름은 서로 다른 보드를 뜻하지 않는다.

| CubeMX 표시 | 뜻 |
|---|---|
| `STM32F411RETx` | NUCLEO 보드에 장착된 MCU 대상 이름 |
| `NUCLEO-F411RE` | 개발보드 전체 이름 |

새 프로젝트를 만들 때 `Board Selector`에서 `NUCLEO-F411RE`를 선택한다. 상단에 `STM32F411RETx - NUCLEO-F411RE`가 표시되면 올바른 보드를 선택한 것이다.

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

## 확정된 공통 통신 핀

세 STM32와 세 ESP32-S3는 모두 같은 UART와 핀을 사용한다.

| 장치 | 기능 | 확정 핀 |
|---|---|---|
| STM32F411RE | USART1 TX | `PA9` |
| STM32F411RE | USART1 RX | `PA10` |
| ESP32-S3 | UART1 TX | `GPIO17` |
| ESP32-S3 | UART1 RX | `GPIO18` |

### 실제 NUCLEO 보드에서 찾는 이름

CubeMX는 MCU 핀 이름을 표시하지만 NUCLEO 보드의 Arduino 헤더에는 `D8`, `D2`가 적혀 있다.

| CubeMX 핀 | 기능 | 보드에 적힌 이름 | Arduino 커넥터 | Morpho 커넥터 |
|---|---|---|---|---|
| `PA9` | `USART1_TX` | `D8` | `CN5` 1번 | `CN10` 21번 |
| `PA10` | `USART1_RX` | `D2` | `CN9` 3번 | `CN10` 33번 |

보드를 ST-LINK USB 커넥터가 위로 오도록 놓았을 때 오른쪽 Arduino 디지털 헤더에서 찾는다.

```text
위쪽 CN5 디지털 헤더
...
D10
D9
D8  ← PA9 / USART1_TX

아래쪽 CN9 디지털 헤더
D7
D6
D5
D4
D3
D2  ← PA10 / USART1_RX
D1
D0
```

따라서 실제 점퍼선은 `D8=TX`, `D2=RX`에 꽂는다. `D8`과 `D2`는 Arduino 헤더 이름이므로 숫자가 서로 떨어져 있어도 정상이다.

연결은 TX와 RX를 서로 교차한다.

```text
STM32 PA9  USART1_TX → ESP32-S3 GPIO18 UART1_RX
STM32 PA10 USART1_RX ← ESP32-S3 GPIO17 UART1_TX
STM32 GND             ↔ ESP32-S3 GND
```

UART 설정도 공통으로 고정한다.

```text
Baud rate: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
Logic level: 3.3V
필수 연결: TX + RX + GND
```

`PA9`, `PA10`, `GPIO17`, `GPIO18`은 센서, ADC, I2C, PWM이나 다른 GPIO 용도로 사용하지 않는다. 불가피한 충돌이 발견되면 한 모듈만 임의로 바꾸지 않고 세 모듈과 통신 펌웨어의 공통 규격을 함께 변경한다.

NUCLEO-F411RE의 USART2 PA2/PA3는 ST-LINK Virtual COM Port를 통한 PC 로그용으로 남겨 둔다. 공통 핀의 자세한 근거와 배선은 [공통 통신 핀 배정](common/PIN_ASSIGNMENT.md)을 참고한다.

## CubeMX 생성 순서

1. STM32CubeMX를 실행한다.
2. `Board Selector`에서 `NUCLEO-F411RE`를 선택한다.
3. 상단에 `STM32F411RETx - NUCLEO-F411RE`가 표시되는지 확인한다.
4. 필요한 센서의 GPIO, ADC, I2C, Timer 등을 설정한다.
5. `USART1`을 `Asynchronous`로 활성화한다.
6. CubeMX 핀 배치가 `PA9=USART1_TX`, `PA10=USART1_RX`인지 확인한다.
7. USART1을 `115200, 8-N-1, Flow control 없음`으로 설정한다.
8. 필요하면 USART2를 PC 로그용으로 설정한다.
9. `Project Manager`에서 자기 모듈의 고유한 프로젝트 이름을 입력한다.
10. 프로젝트 생성 위치가 자기 `stm32/` 폴더 안인지 확인한다.
11. Toolchain 또는 빌드 형식으로 `CMake`를 선택한다.
12. 코드를 생성한다.
13. 생성 직후 아무 기능을 추가하지 않은 상태에서 먼저 빌드한다.

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
- [ ] `USART1`, `PA9 TX`, `PA10 RX`가 설정됐는지 확인
- [ ] 실제 보드에서 `D8=PA9 TX`, `D2=PA10 RX` 위치를 확인
- [ ] `PA9/PA10`을 센서나 다른 기능에 중복 배정하지 않았는지 확인
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
