# NOSTOS 통합 핀 설정

이 문서는 ESP32-S3, NUCLEO-F411RE와 주변 장치 사이의 현재 배선을 정리한다. 모든 디지털
신호는 `3.3V` 로직을 사용하고, 서로 연결된 보드와 모듈의 GND는 공통으로 연결한다.

## ESP32-S3

UART의 송신(`TX`)과 수신(`RX`)은 서로 교차 연결한다.

| ESP32-S3 핀 | STM32 핀 (보드 표기) | STM32 기능 |
| --- | --- | --- |
| GPIO18 (`UART1_RX`) | PA9 (`D8`) | `USART1_TX` |
| GPIO17 (`UART1_TX`) | PA10 (`D2`) | `USART1_RX` |

- UART 설정: `115200 baud`, `8N1`, Hardware Flow Control 비활성화
- ESP32-S3 GND ↔ STM32 GND

## SSD1306

| SSD1306 핀 | STM32 핀 (보드 표기) | STM32 기능 |
| --- | --- | --- |
| SCL | PB8 (`D15`) | `I2C1_SCL` |
| SDA | PB9 (`D14`) | `I2C1_SDA` |

현재 연결 보드에서는 SSD1306만 `I2C1` 버스를 사용한다. MPU6050을 장착하는 다른 노드에서는 같은
버스를 공유할 수 있다. SSD1306의 현재 기본 7-bit 주소는 `0x3C`이다.

## VS1003B

VS1003B의 SPI2 신호와 현재 제어핀은 다음과 같다. 이 MCU 핀들은 NUCLEO 보드의
`D4/D5/D6/D7/D14/D15` 헤더 핀과 동일하지 않으므로, 잘못된 `D` 표기 대신 실제 `CN10`
Morpho 커넥터 위치를 사용한다.

| VS1003B 핀 | STM32 핀 (커넥터) | STM32 기능 |
| --- | --- | --- |
| XRST | PB1 (`CN10-24`) | `GPIO_Output`, 초기 High |
| MISO | PB14 (`CN10-28`) | `SPI2_MISO` |
| MOSI | PB15 (`CN10-26`) | `SPI2_MOSI` |
| SCLK | PB13 (`CN10-30`) | `SPI2_SCK` |
| DREQ | PC5 (`CN10-6`) | `GPIO_Input` |
| XCS | PC6 (`CN10-4`) | `GPIO_Output`, 초기 High |
| XDCS | PC8 (`CN10-2`) | `GPIO_Output`, 초기 High |

`XCS`와 `XDCS`는 Active-Low 신호다. `DREQ`가 High일 때만 새 SCI 명령 또는 SDI 데이터 전송을 시작한다.

## Buttons

버튼은 내부 Pull-up을 사용하며, 버튼을 누르면 입력이 Low가 되도록 GND에 연결한다.

| 버튼 | STM32 핀 (보드 표기) | STM32 기능 | 애플리케이션 역할 |
| --- | --- | --- | --- |
| Button 1 | PB5 (`D4`) | `GPIO_Input` | Speed Up |
| Button 2 | PB10 (`D6`) | `GPIO_Input` | Speed Down |
| Button 3 | PA8 (`D7`) | `GPIO_Input` | Stop Request |
| Button 4 | PC7 (`D9`) | `GPIO_Input` | Buzzer Off (목표) |

현재 펌웨어에서 Button 4는 `MSG_NONE`으로 직접 메시지를 만들지 않으며, `Buzzer Off` 동작은 아직 구현되지 않았다.

## RGB LED

| 채널 | STM32 핀 (보드 표기) | STM32 기능 |
| --- | --- | --- |
| Red | PA4 (`A2`) | `GPIO_Output` |
| Green | PB0 (`A3`) | `GPIO_Output` |
| Blue | PC1 (`A4`) | `GPIO_Output` |

## Buzzer

| 장치 | STM32 핀 (보드 표기) | STM32 기능 |
| --- | --- | --- |
| Buzzer | PB4 (`D5`) | `GPIO_Output` |

## MPU6050 (선택 장치)

| MPU6050 핀 | STM32 핀 (보드 표기) | STM32 기능 |
| --- | --- | --- |
| SCL | PB8 (`D15`) | `I2C1_SCL` |
| SDA | PB9 (`D14`) | `I2C1_SDA` |

현재 연결된 STM32에는 MPU6050이 장착되지 않았고 `MPU6050_SENSOR=OFF`로 빌드한다. MPU6050을
사용하는 다른 노드는 SSD1306과 같은 `I2C1` 배선을 공유하며, 7-bit 주소는 AD0 상태에 따라
`0x68` 또는 `0x69`이다.

## DHT11 (선택 장치)

| DHT11 핀 | STM32 핀 / 전원 | 기능 |
| --- | --- | --- |
| Data | PA1 (`A1`) | 단일 데이터 신호 |
| VCC | `3.3V` | Power |
| GND | GND | Ground |

현재 연결된 STM32에는 DHT11이 장착되지 않았고 `DHT11_SENSOR=OFF`로 빌드한다.

## 핀 중복 확인

- PB8/PB9는 현재 SSD1306이 사용한다. MPU6050 장착 노드에서는 공용 `I2C1` 버스로 정상 공유한다.
- 그 외 현재 MCU 핀은 장치 사이에 중복 할당되지 않았다.
- NUCLEO `D4`, `D5`, `D6`, `D7`, `D14`, `D15` 헤더는 각각 버튼·부저·I2C에 사용한다.
- VS1003B는 위 표의 STM32 MCU 핀과 `CN10` 위치를 기준으로 연결하며 잘못된 `D` 표기로 연결하지 않는다.

## 현재 연결 STM32 구성

- MCU: STM32F411xC/xE (`chip ID 0x431`)
- 장착: VS1003B, SSD1306
- 미장착·비활성화: MPU6050, DHT11
- 정확한 ST-LINK serial은 Git에 기록하지 않고 `firmware/inventory/boards.local.json`에서만 관리한다.

## 근거

- [STM32 GPIO 핀 정의](firmware/stm32/Core/Inc/main.h)
- [STM32 GPIO와 주변장치 초기화](firmware/stm32/Core/Src/main.c)
- [STM32 Alternate Function 설정](firmware/stm32/Core/Src/stm32f4xx_hal_msp.c)
- [현재 CubeMX 설정](firmware/stm32/nostos_stm32.ioc)
- [ESP32-S3 UART1 핀과 런타임 설정](firmware/esp32/main/bridge_runtime.c)
- [ESP32-S3 ↔ STM32 UART 배선](firmware/esp32/README.md)
- [버튼 애플리케이션 역할](firmware/stm32/MyApp/hw/button.c)
- [DHT11 핀 설정](firmware/stm32/MyApp/common/app_config.h)
- [NUCLEO-F411RE D/A 헤더·Morpho 커넥터 핀 배치](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
- [STM32F411RE 데이터시트](https://www.st.com/resource/en/datasheet/stm32f411re.pdf)
- [VS1003B 데이터시트](https://www.vlsi.fi/fileadmin/datasheets/vs1003.pdf)
