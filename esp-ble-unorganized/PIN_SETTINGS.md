# Pin Settings

## Bluetooth (ESP32) ↔ USART (STM32)

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| STM32 USART1 TX | D8 | **PA9** | USART1_TX | ESP32 통신 RX |
| STM32 USART1 RX | D2 | **PA10** | USART1_RX | Pico GP0 TX |

## 자이로 센서

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| MPU6050 Clock | D15 | **PB8** | I2C1_SCL | SCL |
| MPU6050 Data | D14 | **PB9** | I2C1_SDA | SDA |

## Ultrasonic

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| HC-SR04 Echo | A0 | **PA0** | TIM2_CH1 Input Capture | ECHO |
| HC-SR04 Trigger | A5 | **PC0** | GPIO_Output | TRIG |

## 버튼 (1-4)

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| 버튼 1 | D4 | **PB5** | GPIO/EXTI | 속도 up |
| 버튼 2 | D6 | **PB10** | GPIO/EXTI | 속도 down |
| 버튼 3 | D7 | **PA8** | GPIO/EXTI | 안전운전 알림 |
| 버튼 4 | D9 | **PC7** | GPIO/EXTI | 정지 요청 |

## RGB

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| RGB LED R PIN | A2 | **PA4** | GPIO_Output | RGB R |
| RGB LED G PIN | A3 | **PB0** | GPIO_Output | RGB G |
| RGB LED B PIN | A4 | **PC1** | GPIO_Output | RGB B |

## Alarming by Buzzer

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| 부저 | D5 | **PB4** | GPIO_Output | Buzzer |

## 오디오 디코더/코덱 칩(IC)

| 담당 | 보드 표기 | STM32 핀 | CubeMX 기능 | 연결 대상 |
| --- | --- | --- | --- | --- |
| VS1003B Clock | Morpho | **PB13** | SPI2_SCK | SCLK |
| VS1003B MISO | Morpho | **PB14** | SPI2_MISO | SO |
| VS1003B MOSI | Morpho | **PB15** | SPI2_MOSI | SI |
| VS1003B Control CS | Morpho | **PB12** | GPIO_Output | XCS |
| VS1003B Data CS | Morpho | **PC5** | GPIO_Output | XDCS |
| VS1003B Ready | Morpho | **PC4** | GPIO_Input | DREQ |
| VS1003B Reset | Morpho | **PB1** | GPIO_Output | XRST |
