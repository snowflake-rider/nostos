# 공통 통신 핀 배정

세 모듈은 같은 NUCLEO-F411RE와 ESP32-S3 DevKitC-1을 사용하므로 STM32와 ESP32-S3 사이 UART 핀을 공통으로 고정한다.

## 확정 핀

| 장치 | 주변장치 | 기능 | 핀 | 예약 상태 |
|---|---|---|---|---|
| STM32F411RE | USART1 | TX | `PA9` | 통신 전용 |
| STM32F411RE | USART1 | RX | `PA10` | 통신 전용 |
| ESP32-S3 | UART1 | TX | `GPIO17` | 통신 전용 |
| ESP32-S3 | UART1 | RX | `GPIO18` | 통신 전용 |

## 실제 배선

TX는 상대 보드의 RX에 연결한다.

```text
NUCLEO-F411RE                    ESP32-S3 DevKitC-1
PA9  / USART1_TX  ------------> GPIO18 / UART1_RX
PA10 / USART1_RX  <------------ GPIO17 / UART1_TX
GND                 ----------- GND
```

## 공통 UART 설정

```text
Mode: Asynchronous
Baud rate: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
Logic level: 3.3V
```

## 팀 규칙

1. 모든 STM32 CubeMX 프로젝트에서 `USART1`을 활성화한다.
2. 모든 STM32에서 `PA9=USART1_TX`, `PA10=USART1_RX`로 설정한다.
3. `PA9`와 `PA10`은 센서, ADC, I2C, PWM이나 일반 GPIO에 사용하지 않는다.
4. 모든 ESP32-S3 통신 펌웨어에서 `GPIO17=TX`, `GPIO18=RX`로 설정한다.
5. `GPIO17`과 `GPIO18`도 다른 주변장치에 사용하지 않는다.
6. UART 연결에는 TX와 RX뿐 아니라 GND도 반드시 연결한다.
7. 초기 시험에서는 STM32와 ESP32-S3에 각각 USB 전원을 공급하고 전원 핀끼리는 연결하지 않는다.
8. 핀 충돌이 불가피하면 한 모듈만 임의로 변경하지 않고 팀 공통 문서와 세 노드 설정을 함께 변경한다.

## PC 로그용 UART

NUCLEO-F411RE의 USART2 PA2/PA3는 ST-LINK Virtual COM Port를 통한 PC 로그용으로 남겨 둔다.

```text
USART1 PA9/PA10 → ESP32-S3 통신
USART2 PA2/PA3  → ST-LINK USB를 통한 PC 로그
```

## 공식 근거

- [STMicroelectronics STM32F411RE 데이터시트](https://www.st.com/resource/en/datasheet/stm32f411re.pdf): `PA9=USART1_TX`, `PA10=USART1_RX`
- [STMicroelectronics NUCLEO-64 사용자 매뉴얼 UM1724](https://www.st.com/resource/en/user_manual/dm00105823.pdf): NUCLEO-F411RE 헤더에서 `PA9`와 `PA10` 사용 가능
- [Espressif ESP32-S3 DevKitC-1 v1.1 사용자 가이드](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html): `GPIO17=U1TXD`, `GPIO18=U1RXD`
