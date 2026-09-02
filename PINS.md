# NOSTOS 핀 설정

모든 디지털 신호는 `3.3V` 로직이며 연결된 보드·모듈은 GND를 공유합니다.

| 장치·신호 | STM32 핀 (보드/커넥터) | 기능·현재 상태 |
| --- | --- | --- |
| ESP32-S3 UART RX GPIO18 | PA9 (`D8`) | `USART1_TX` |
| ESP32-S3 UART TX GPIO17 | PA10 (`D2`) | `USART1_RX` |
| SSD1306 SCL | PB8 (`D15`) | `I2C1_SCL`, 현재 장착 |
| SSD1306 SDA | PB9 (`D14`) | `I2C1_SDA`, 주소 `0x3C` |
| VS1003B XRST | PB1 (`CN10-24`) | 출력, 초기 High |
| VS1003B MISO | PB14 (`CN10-28`) | `SPI2_MISO` |
| VS1003B MOSI | PB15 (`CN10-26`) | `SPI2_MOSI` |
| VS1003B SCLK | PB13 (`CN10-30`) | `SPI2_SCK` |
| VS1003B DREQ | PC5 (`CN10-6`) | 입력; High일 때만 SCI/SDI 전송 시작 |
| VS1003B XCS | PC6 (`CN10-4`) | Active-Low 출력, 초기 High |
| VS1003B XDCS | PC8 (`CN10-2`) | Active-Low 출력, 초기 High |
| Button 1 | PB5 (`D4`) | 내부 Pull-up, Low=Speed Up |
| Button 2 | PB10 (`D6`) | 내부 Pull-up, Low=Speed Down |
| Button 3 | PA8 (`D7`) | 내부 Pull-up, Low=Stop Request |
| Button 4 | PC7 (`D9`) | 내부 Pull-up; 메시지를 보내지 않고 로컬 alert·buzzer·audio·display 출력 상태 reset |
| RGB Red | PA4 (`A2`) | 출력 |
| RGB Green | PB0 (`A3`) | 출력 |
| RGB Blue | PC1 (`A4`) | 출력 |
| Buzzer | PB4 (`D5`) | 출력 |
| MPU6050 SCL | PB8 (`D15`) | Node 3 선택 장치, `I2C1_SCL` |
| MPU6050 SDA | PB9 (`D14`) | Node 3 선택 장치, `I2C1_SDA`; 주소 `0x68`/`0x69` |
| DHT11 Data | PA1 (`A1`) | Node 2 선택 장치, 단일 데이터 신호 |
| DHT11 VCC/GND | `3.3V`/GND | Node 2 선택 장치 전원 |

UART는 TX/RX 교차 연결, `115200 baud`, `8N1`, Hardware Flow Control 비활성화입니다. VS1003B는
NUCLEO `D4/D5/D6/D7/D14/D15`가 아니라 표의 실제 MCU 핀과 `CN10` 위치로 연결합니다.

PB8/PB9는 SSD1306과 Node 3의 MPU6050이 공유하는 `I2C1`이며 다른 핀 중복은 없습니다. 세 STM32는
VS1003B·SSD1306을 공통으로 사용하고, release variant는 Node 1 `node1-base`(두 로컬 센서 OFF), Node 2
`node2-dht11`, Node 3 `node3-mpu6050`으로 분리됩니다. 이 구성은 빌드로 검증됐지만 세 보드의 센서·출력
실물 동작은 아직 검증되지 않았습니다. ST-LINK serial은 Git에 두지 않고
`firmware/inventory/boards.local.json`에서만 관리합니다.

## 근거

- [GPIO 정의와 초기화](firmware/stm32/Core/Inc/main.h), [Alternate Function](firmware/stm32/Core/Src/stm32f4xx_hal_msp.c), [CubeMX 설정](firmware/stm32/nostos_stm32.ioc)
- [ESP32 UART 설정](firmware/esp32/main/bridge_runtime.c), [버튼 역할](firmware/stm32/MyApp/hw/button.c), [DHT11 설정](firmware/stm32/MyApp/common/app_config.h)
- [NUCLEO-F411RE 핀 배치](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf), [STM32F411RE](https://www.st.com/resource/en/datasheet/stm32f411re.pdf), [VS1003B](https://www.vlsi.fi/fileadmin/datasheets/vs1003.pdf)
