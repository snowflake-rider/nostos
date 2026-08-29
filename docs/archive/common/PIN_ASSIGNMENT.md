> 이관 원문: `stm32-project/common/PIN_ASSIGNMENT.md`. 현재 실행 경로는 [팀원 시작 안내](../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 공통 USART 핀 배정

> **현재 단일 버튼 실물 시험(2026-08-28)**: 통합 STM32 펌웨어는 사용자 지정에 따라
> **D10/PB6 버튼, D8/PA9 USART1_TX, D2/PA10 USART1_RX**로 변경했다.
> USART2는 ST-LINK USB 진단용 송신 복사본만 출력한다. D0/D1은 ESP32에 연결하지 않는다.
> [현재 배선·보드 브리지 주의사항·검증](../../verification/stm32-button-uart.md)을 따른다.
> 아래는 이전 두 STM32 간 시험이며, 현재 상대 장치는 ESP32(GPIO18 RX/GPIO17 TX)다.

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
- 현재 메시지 ID는 `common/protocol/message_type.h`에 정의합니다.
- UART 전송 시 enum 객체가 아니라 명시적으로 변환한 `uint8_t` 한 바이트를 보냅니다.

PA9와 PA10은 공용 통신용으로 예약하며 센서나 일반 GPIO로 사용하지 않습니다. 향후 다른 UART 장치를 연결하더라도 STM32 측 설정과 메시지 규격은 이 문서를 기준으로 확장합니다.
