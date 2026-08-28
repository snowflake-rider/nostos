> 이관 원문: `stm32-project/CUBEMX_SETUP.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# STM32CubeMX 및 빌드 설정

## 공용 프로젝트

새 CubeMX 프로젝트를 만들지 않고 아래 파일을 공통으로 사용합니다.

```text
integration/stm32/bike_swarm_guard.ioc
```

대상 보드는 `NUCLEO-F411RE`, MCU는 `STM32F411RETx`입니다.

## 주요 핀

| 기능 | 핀 |
|---|---|
| HC-SR04 TRIG | `PC0` |
| HC-SR04 ECHO | `PA0` |
| RGB R/G/B | `PA4` / `PB0` / `PC1` |
| VS1003B DREQ/XDCS/XCS/RST | `PC4` / `PC5` / `PB12` / `PB1` |
| VS1003B SPI2 SCK/MISO/MOSI | `PB13` / `PB14` / `PB15` |
| 버튼 1~4 | `PB5` / `PB10` / `PA8` / `PC7` |
| 액티브 부저 | `PB4` |
| MPU6050 I2C1 SCL/SDA | `PB8` / `PB9` |
| USART1 TX/RX | `PA9` / `PA10` |

I2C1은 100kHz, USART1은 `115200, 8-N-1, Flow control 없음`을 사용합니다.

## CubeMX 코드 생성

1. `bike_swarm_guard.ioc`를 엽니다.
2. 핀 충돌이 없는지 확인합니다.
3. `Project Manager > Code Generator > Keep User Code`를 활성화합니다.
4. CMake 형식을 유지하고 Generate Code를 실행합니다.
5. `USER CODE BEGIN/END` 밖의 생성 코드를 수동 편집하지 않습니다.
6. 생성 직후 전체 빌드를 실행합니다.

## CMake 빌드

STM32Cube 확장의 Build 기능을 사용하거나 `integration/stm32`에서 Debug preset을 사용합니다. 빌드 결과는 `integration/stm32/build` 아래에 생성되며 Git에는 포함되지 않습니다.

성공 기준:

- 컴파일 및 링크 오류 없음
- RAM/Flash 사용량 출력
- `bike_swarm_guard.elf` 생성

## 하드웨어 주의사항

- 두 STM32를 연결할 때 TX와 RX를 교차하고 GND를 공통 연결합니다.
- 각각 USB 전원을 사용하면 보드 사이의 5V/3.3V 전원 핀은 연결하지 않습니다.
- HC-SR04는 5V로 구동되므로 ECHO 신호는 최종 배선에서 3.3V 수준으로 낮추는 것을 권장합니다.
- MPU6050은 우선 3.3V로 공급합니다.
