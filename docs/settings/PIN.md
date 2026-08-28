> 이관 원문: `settings/PIN.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# STM32 ↔ ESP32 핀 설정

기록일: 2026-08-28.

## 현재 사용 연결

```text
STM32 D8 / PA9 / USART1_TX → ESP32 GPIO18 / UART1_RX
STM32 GND                ↔ ESP32 GND
Mac USB                  → ESP32 전원
```

사용자가 테스트 성공을 보고한 뒤, 연결을 `STM32 D8 → ESP32 pin18`이라고 설명했다.
ESP32 쪽은 사용자가 GPIO18로 추정했으며, **현재 펌웨어가 사용하는 RX는 GPIO18**이다.
실물 핀 위치를 사진으로 대조하거나 최종 성공 로그를 재수집한 것은 아니다.
따라서 이 기록을 모든 Mesh 노드의 수신까지 독립 검증했다는 뜻으로 읽지 않는다.

## 배선표

| 용도 | STM32 NUCLEO-F411RE | 연결 대상 | 비고 |
| --- | --- | --- | --- |
| 버튼 이벤트 송신 | **D8 / PA9 / USART1_TX** | **ESP32 GPIO18 / UART1_RX** | 현재 단방향 시험의 데이터 선 |
| 공통 접지 | **GND** | **ESP32 GND** | ESP32가 Mac USB로 전원을 받아도 이 연결 유지 |
| 외부 버튼 | **D10 / PB6** | **버튼 → STM32 GND** | GPIO Input + Pull-up, 누르면 LOW |
| 반대 방향 데이터 | **D2 / PA10 / USART1_RX** | **ESP32 GPIO17 / UART1_TX** | 양방향 통신 시 사용. 현재 단방향 시험에는 불필요 |

ESP32 세 대는 모두 **ESP32-S3-N16R8**이다(사용자 확인).
`D6`, `76`, `B6`는 로그에서 보드를 구분하는 이름이지 배선할 GPIO 이름이 아니다.

## UART 설정

| 항목 | STM32 | ESP32 |
| --- | --- | --- |
| 주변장치 | USART1 | UART1 |
| TX | PA9 / D8 | GPIO17 |
| RX | PA10 / D2 | GPIO18 |
| 통신 속도 | 115200 baud | 115200 baud |
| 프레임 | 8 data bits, no parity, 1 stop bit | 동일 |
| Hardware flow control | 없음 | 없음 |

외부 버튼 이벤트는 문자열 `"13"`이 아니라 바이너리 **1바이트 `0x13`**이다.

STM32 **D0/PA3·D1/PA2(USART2)는 현재 ESP32 연결에 사용하지 않는다.**
현재 STM32 펌웨어에서 USART2는 ST-LINK USB 진단 복사본에 사용한다.
USB 로그만으로 ESP32의 UART 수신까지 확인됐다고 판단하지 않는다.

## ESP32 핀 번호 주의

| 핀 이름 | GPIO 번호 | 모듈 패드 번호 | 이번 UART 연결 |
| --- | ---: | ---: | --- |
| IO18 | **18** | 11 | STM32에서 받는 RX |
| IO17 | **17** | 10 | STM32로 보내는 TX |
| IO10 | 10 | **18** | 사용하지 않음 |
| RXD0 | 44 | 36 | UART0이므로 사용하지 않음 |
| TXD0 | 43 | 37 | UART0이므로 사용하지 않음 |

**GPIO18은 헤더의 18번째 핀이나 모듈 패드 18번이 아니다.**
개발보드의 GPIO 인쇄(`18` / `IO18`)와 해당 보드 핀맵을 기준으로 확인한다.

## 전원과 변경 시 주의

- 각 보드는 USB로 전원을 공급한다. ESP32 전원을 STM32에서 받을 필요는 없다.
- 보드 사이에는 GND를 공통으로 연결하되, **3V3/5V 전원 레일끼리는 연결하지 않는다.**
- 배선을 바꿀 때는 전원을 끈다. 성공한 연결은 불필요하게 옮기지 않는다.
- 온보드 RGB LED 핀은 이 UART 배선과 별개이며, N16R8 이름만으로 GPIO38/48을 확정하지 않는다.

## 관련 자료

- [전체 ESP32 모듈 핀 정의표와 원본 이미지](../03-reference/ESP32-S3-N16R8-PINOUT.md)
- [ESP32 Layer 8 실제 UART 설정 소스](../../code/layers/layer-8/main/bridge_runtime.c)
- [STM32 초기화 소스](../../code/firmware/stm32/Core/Src/main.c)
- [읽기 전용 UART 진단](../layers/layer-8/UART_DIAGNOSTICS.md)
- [빠른 버튼 → Mesh 시험](../layers/layer-8/FAST_CHECK.md)
