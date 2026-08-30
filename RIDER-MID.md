# RIDER-MID

Rider Head와 같은 기본 장치에 DHT11을 추가한 Rider Mid 구성이다.

| 구분 | 장치 | 연결 / 상태 |
| --- | --- | --- |
| Main MCU | NUCLEO-F411RE | STM32F411xC/xE, chip ID `0x431` |
| 통신 | ESP32-S3 | USART1: PA9(TX), PA10(RX) |
| 오디오 | VS1003B | SPI2 + XRST PB1, DREQ PC5, XCS PC6, XDCS PC8 |
| 화면 | SSD1306 | I2C1 PB8/PB9, 활성화 |
| 입력 | Button 1~4 | PB5, PB10, PA8, PC7 |
| 출력 | RGB LED | PA4, PB0, PC1 |
| 출력 | Buzzer | PB4 |

## 센서

| 센서 | 연결 / 펌웨어 설정 |
| --- | --- |
| MPU6050 | 미장착, `MPU6050_SENSOR=OFF` |
| DHT11 | PA1, 장착, `DHT11_SENSOR=ON` |

정확한 ST-LINK serial은 Git 문서에 기록하지 않고 로컬 장치 inventory에서만 관리한다.
