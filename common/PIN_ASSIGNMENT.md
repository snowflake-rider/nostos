# 공통 USART 핀 배정

현재 통합 시험에서는 NUCLEO-F411RE 두 대를 USART1로 직접 연결합니다.

## 확정 설정

| 기능 | 설정 |
|---|---|
| STM32 TX | `PA9 / USART1_TX` |
| STM32 RX | `PA10 / USART1_RX` |
| Baud rate | `115200` |
| Data bits | `8` |
| Parity | `None` |
| Stop bits | `1` |
| Flow control | `None` |
| Logic level | `3.3V` |

## STM32 두 대 연결

```text
보드 A PA9  TX  ─────→ 보드 B PA10 RX
보드 A PA10 RX  ←───── 보드 B PA9  TX
보드 A GND      ────── 보드 B GND
```

두 보드를 각각 USB로 공급할 때 5V나 3.3V 전원 핀끼리는 연결하지 않습니다.

## 메시지 동작

- 이 보드에서 생성된 버튼·센서 메시지는 로컬 처리 후 USART로 한 번 전송합니다.
- USART로 받은 메시지는 로컬에서만 처리하고 다시 전송하지 않습니다.
- 현재 메시지 ID는 `integration/stm32/MyApp/common/message_type.h`에 정의합니다.
- UART 전송 시 enum 객체가 아니라 명시적으로 변환한 `uint8_t` 한 바이트를 보냅니다.

PA9와 PA10은 공용 통신용으로 예약하며 센서나 일반 GPIO로 사용하지 않습니다. 향후 다른 UART 장치를 연결하더라도 STM32 측 설정과 메시지 규격은 이 문서를 기준으로 확장합니다.
