# NOSTOS 핀 설정

## ESP32-S3 ↔ STM32 USART1

UART의 송신(`TX`)과 수신(`RX`)은 서로 교차 연결한다.

| ESP32-S3 | STM32 보드 표기 | STM32 기능 |
| --- | --- | --- |
| GPIO18 (`UART1_RX`) | D8 (`PA9`) | `USART1_TX` |
| GPIO17 (`UART1_TX`) | D2 (`PA10`) | `USART1_RX` |

두 보드의 GND도 서로 연결한다. UART 설정은 `115200 baud`, `8N1`, Hardware Flow Control 비활성화, `3.3V` 로직이다.

## SSD1306 ↔ STM32 I2C1

| STM32 보드 표기 | SSD1306 |
| --- | --- |
| D15 (`PB8`, `I2C1_SCL`) | SCL |
| D14 (`PB9`, `I2C1_SDA`) | SDA |

SSD1306과 STM32의 GND를 공통으로 연결한다. 현재 펌웨어에서 SSD1306과 MPU6050은 같은 `I2C1` 버스를 공유한다.

## VS1003B ↔ NUCLEO-F411RE

### 현재 활성 배선

현재 펌웨어와 CubeMX 설정에 실제로 적용된 배선은 다음과 같다.

| VS1003B | 방향 | STM32 | CN10 | 현재 설정 |
| --- | --- | --- | ---: | --- |
| XDCS / DCS | STM32 → VS1003B | PC5 | 6 | GPIO_Output, 초기 High |
| XCS / CS | STM32 → VS1003B | PB12 | 16 | GPIO_Output, 초기 High |
| GND | 공통 | GND | 20 | 공통 접지 |
| XRST / RST | STM32 → VS1003B | PB1 | 24 | GPIO_Output, 초기 High |
| MOSI / SI | STM32 → VS1003B | PB15 | 26 | SPI2_MOSI |
| MISO / SO | VS1003B → STM32 | PB14 | 28 | SPI2_MISO |
| SCLK / SCK | STM32 → VS1003B | PB13 | 30 | SPI2_SCK |
| DREQ | VS1003B → STM32 | PC4 | 34 | GPIO_Input |

아래의 PC8·PC6·PC5 제어 핀 배치는 배선을 정리하기 위한 **대체 제안이며 현재 펌웨어에는
적용되지 않았다.** 실제 변경 시 `.ioc`, GPIO 매크로와 초기화를 함께 수정하고 다시 검증해야 한다.

### SPI2 핀 제약

VS1003B의 `MISO`, `MOSI`, `SCLK`는 같은 STM32 하드웨어 SPI 주변장치의 Alternate Function 조합을 사용해야 한다.

현재 NOSTOS 펌웨어는 `SPI2`와 `HAL_SPI_*()`를 사용하므로 다음 세 핀을 유지한다.

| VS1003B | STM32 | CubeMX 기능 |
| --- | --- | --- |
| MISO / SO | PB14 | SPI2_MISO |
| MOSI / SI | PB15 | SPI2_MOSI |
| SCLK / SCK | PB13 | SPI2_SCK |

### 검토한 대체 배치

다음 배치는 그대로 사용할 수 없다.

| VS1003B | 제안 핀 | 판정 | 이유 |
| --- | --- | --- | --- |
| XRST | PC8 | 사용 가능 | 일반 GPIO 출력 가능 |
| MISO | PC6 | 사용 불가 | `SPI2_MISO` Alternate Function이 없음 |
| MOSI | PC5 | 사용 불가 | SPI MOSI Alternate Function이 없음 |
| SCLK | PA12 | 사용 불가 | `SPI2_SCK`가 아니며 USB FS DP와 공유되는 핀 |
| DREQ | PA11 | 조건부 가능 | GPIO 입력 가능, USB FS DM 사용 시 충돌 |
| XCS | PB12 | 사용 가능 | 일반 GPIO 출력 가능 |
| XDCS | PB2 | 주의해서 가능 | GPIO 출력 가능, BOOT1 기능 주의 |

PC6·PC5·PA12를 사용하려면 하드웨어 SPI2가 아닌 별도의 Software SPI 드라이버가 필요하다. 현재 VS1003B 드라이버를 다시 작성해야 하므로 권장하지 않는다.

### 제어 핀 대체안 후보

제어 핀 선택을 최대한 유지해야 한다면 SPI 세 핀만 다음과 같이 복원한다.

| VS1003B | STM32 | CubeMX 설정 |
| --- | --- | --- |
| XRST | PC8 | GPIO_Output, Initial Output Level High |
| MISO / SO | PB14 | SPI2_MISO |
| MOSI / SI | PB15 | SPI2_MOSI |
| SCLK / SCK | PB13 | SPI2_SCK |
| DREQ | PA11 | GPIO_Input, No pull |
| XCS | PB12 | GPIO_Output, Initial Output Level High |
| XDCS | PB2 | GPIO_Output, Initial Output Level High |

이 수정안은 가능한 후보지만 현재 펌웨어에 적용되지 않았다. PA11의 USB 충돌 가능성과 PB2의
BOOT1 기능도 고려해야 한다.

### 배선 정리용 대체 제안

USB·BOOT1 핀을 피하고 CN10에서 배선을 두 묶음으로 정리하려면 다음 배치를 사용한다.
이 표 역시 **미적용 제안**이며 위의 현재 활성 배선과 혼용하면 안 된다.

| VS1003B | 방향 | STM32 | CN10 | CubeMX 설정 |
| --- | --- | --- | ---: | --- |
| XDCS / DCS | STM32 → VS1003B | PC8 | 2 | GPIO_Output, Initial Output Level High |
| XCS / CS | STM32 → VS1003B | PC6 | 4 | GPIO_Output, Initial Output Level High |
| DREQ | VS1003B → STM32 | PC5 | 6 | GPIO_Input, No pull |
| GND | 공통 | GND | 20 | 공통 접지 |
| XRST / RST | STM32 → VS1003B | PB1 | 24 | GPIO_Output, Initial Output Level High |
| MOSI / SI | STM32 → VS1003B | PB15 | 26 | SPI2_MOSI |
| MISO / SO | VS1003B → STM32 | PB14 | 28 | SPI2_MISO |
| SCLK / SCK | STM32 → VS1003B | PB13 | 30 | SPI2_SCK |

| STM32 | VS1003B |
| --- | --- |
| PC8 | XDCS |
| PC6 | XCS |
| PC5 | DREQ |
| PB1 | XRST |
| PB15 | MOSI |
| PB14 | MISO |
| PB13 | SCLK |

### CubeMX SPI2 설정

| 항목 | 설정 |
| --- | --- |
| Mode | Full-Duplex Master |
| Hardware NSS | Disable / Software NSS |
| Data Size | 8 Bits |
| First Bit | MSB First |
| Clock Polarity | Low |
| Clock Phase | 1 Edge |
| 초기 Baud Rate Prescaler | 16 (`16 MHz / 16 = 1 MHz`) |

`XCS`와 `XDCS`는 Active-Low 신호이므로 초기 출력 상태를 High로 설정한다. `DREQ`가 High일 때만 새로운 SCI 명령 또는 SDI 데이터 전송을 시작한다.

### 변경 시 주의사항

- PB8은 MPU6050·SSD1306 공용 `I2C1_SCL`이므로 VS1003B에 사용하지 않는다.
- 핀 매크로만 수정하지 않는다. `.ioc`, `main.h`, `MX_GPIO_Init()`의 포트별 초기화가 서로 일치해야 한다.
- 특히 XCS를 PB12에서 PC6으로 옮기면 GPIOB에 묶여 있는 기존 초기화 코드도 GPIOC로 이동해야 한다.
- `U5V`는 GPIO가 아니다. VS1003B 모듈이 5V 입력을 지원한다고 명시된 경우에만 전원 입력으로 검토한다.
- VS1003B의 `GBUF`는 GND가 아니다.

### 근거

- [ESP32-S3 UART1 핀과 런타임 설정](firmware/esp32/main/bridge_runtime.c)
- [ESP32-S3 ↔ STM32 UART 배선](firmware/esp32/README.md)
- [현재 STM32 I2C1·SPI2·USART1 초기화](firmware/stm32/Core/Src/main.c)
- [현재 STM32 I2C1·SPI2·USART1 GPIO Alternate Function](firmware/stm32/Core/Src/stm32f4xx_hal_msp.c)
- [현재 CubeMX 설정](firmware/stm32/nostos_stm32.ioc)
- [STM32F411RE 데이터시트](https://www.st.com/resource/en/datasheet/stm32f411re.pdf)
- [VS1003B 데이터시트](https://www.vlsi.fi/fileadmin/datasheets/vs1003.pdf)
