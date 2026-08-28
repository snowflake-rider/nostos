> 이관 원문: `stm32-project/integration/stm32/BUTTON_UART_TEST.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# D10 버튼 → USART1 → ESP32 시험

## 현재 설정: USART1 D8/PA9 (2026-08-28 변경)

- D10/PB6 버튼은 그대로 유지한다.
- **D8/PA9 USART1 TX → ESP32 GPIO18 RX**, GND ↔ GND.
- 역방향을 연결할 때는 D2/PA10 USART1 RX ← ESP32 GPIO17 TX.
- USART1은 115200/8-N-1이며 `app_init()`에 `huart1`을 전달한다.
- USART2는 ST-LINK USB 진단용이다. USART1 HAL 송신 성공 후 같은 바이트를
  USB로 한 번 복사한다. 복사 실패는 원래 전송을 재시도하지 않는다.
- D0/D1에는 ESP32 UART 선을 연결하지 않는다. 보드 납땜 브리지는 변경하지 않았다.
- 이전 Flash 전체 백업: `build/pre-usart1.HMU6Pv/previous-flash.bin`.
- 새 ELF SHA-256: `4b5d111ce16c5d211d921b62c14af8e7bee1da68254399973cd523a19bb89cd7`.
- 설치 검증: CubeProgrammer `Download verified successfully`, MCU reset.
- 실행 중 PA9/PA10 AF7, USART1 BRR=0x8B, CR1=0x202C 확인.
- 디버거 TX 카운터 주소는 새 ELF에서 `0x200001e0`이다. 이전 주소를 재사용하지 않는다.
- 전환 후 실물 시험: USART1 송신 성공 USB 복사본 `0x13` 5회 관찰.
  연결 ESP32(D6)의 `0x13` 수신은 0회, 다른 유효 ID `0x20` 1회와 invalid 4회 증가.
  따라서 USART1 전환·설치는 완료했지만 STM32 → ESP32 버튼 전달은 아직 미검증이다.
  수신 보드 B6의 USB가 분리되어 해당 시험은 STM32/D6/76만 관찰했다.
  ESP32 펌웨어·Mesh 키는 변경하지 않았으며 LED 점멸 기능도 아직 추가하지 않았다.

아래는 이전 USART2 시험 기록이며 현재 외부 배선이 아니다.

## 이전 USART2 시험 기록

현재 통합 STM32 펌웨어의 단일 외부 버튼 시험 설정 (2026-08-28).

| 기능 | 보드 핀 | MCU 설정 |
| --- | --- | --- |
| 테스트 버튼 | D10 | PB6, GPIO Input, Pull-up, active-low |
| UART TX | D1 | PA2, USART2_TX, AF7 |
| UART RX | D0 | PA3, USART2_RX, AF7 |
| UART 형식 | — | 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control |

버튼은 **D10/PB6 → 스위치 → GND**로 연결한다. 30ms 안정된 눌림 한 번마다
`MSG_STOP_REQUEST` (`0x13`)를 **바이너리 한 바이트**로 보낸다. 문자열 `"13"`이 아니다.
계속 누르고 있으면 재전송하지 않으며, 놓은 뒤 다시 누르면 새 이벤트가 된다.
부팅할 때 이미 눌려 있으면 놓았다 다시 눌러야 한다.

기존 BTN1/PB5, BTN2/PB10, BTN3/PA8, BTN4/PC7 입력과 센서·출력 코드는 보존했다.
이번 추가 입력은 TEST_BUTTON이며, 보드 파란 USER 버튼 PC13은 변경하지 않았다.
통신에 전달하는 HAL handle은 `huart2`, IRQ는 `USART2_IRQHandler`다.

## ESP32-S3 Layer8 배선

```text
STM32 D1 / PA2 TX  → ESP32 GPIO18 / UART1 RX
STM32 D0 / PA3 RX  ← ESP32 GPIO17 / UART1 TX  (역방향 수신도 할 때)
STM32 GND         ─ ESP32 GND
```

ESP32 쪽 UART 번호는 계속 UART1이며, STM32 쪽만 USART2로 바꾼 것이다.
버튼 방송만 시험할 때도 공통 GND는 필요하다. 서로의 5V/3.3V 전원 핀은 연결하지 않는다.

**NUCLEO-F411RE 보드의 실제 D0/D1 경로는 소스 설정과 별개다.**
ST UM1724 §7.10에 따르면 외부 D0/D1용 경로는 SB62/SB63 ON,
ST-LINK VCP 경로는 SB13/SB14 OFF가 필요하다. 브리지는 소프트웨어로 바꿀 수 없다.
현재 보드의 납땜 상태는 확인되지 않았다. 실제 ESP32 수신이 없으면 배선·브리지를 확인하고,
ESP32 TX와 ST-LINK TX가 같은 STM32 RX를 동시에 구동하지 않도록 한다.
[ST 보드 설명서](https://www.st.com/resource/en/user_manual/um1724-.pdf#page=26).

## 검사와 설치

- Host Debug / Release / ASan+UBSan: 각 3/3 PASS.
- 새 PB6 시험은 수정 전 실패, 수정 후 통과했다.
- 실제 `button_get_message()`와 `uart_service_send_message()`를 검사하며 HAL 하드웨어만 대체했다.
- 검사 항목: 29/30ms 경계, 한 바이트 0x13, 길게 누르기, 재누르기,
  바운스, 부팅 시 눌림, tick wraparound, UART 실패, 기존 버튼 4개.
- 실제 STM32 Debug 빌드: Flash 123KB / 512KB, RAM 2240B / 128KB.
- ELF SHA-256: `1f64dbb3919ef8b25a50126aa7601bc7a8d7fbea8c914a357c51cfc0f70d993e`.
- 설치 대상: NUCLEO-F411RE, ST-LINK SN `066DFF485277504867161930`.
- CubeProgrammer: `Download verified successfully`, MCU reset 확인.
- 설치 전 512KB Flash 백업: `build/pre-pb6-usart2.PYyuHl/previous-flash.bin` (소유자 전용).
- 실행 중 GPIOB/PB6 Input+Pull-up, GPIOA PA2/PA3 AF7, USART2 BRR=`0x8B`,
  CR1=`0x202C` 확인. STM32 TX 카운터 1건 관측.

IOC와 초기화 소스는 함께 수정하고 핀·주변장치·baud 일치 여부를 검사했다.
이번에는 CubeMX 재생성 대신 기존 생성 파일의 해당 부분만 수정했다.
호스트 시험과 Flash verify만으로 실제 UART 배선이나 Mesh 수신을 보증하지 않는다.

## 실제 전송 확인 순서

1. 외부 PB6 버튼을 한 번 눌렀다 놓는다.
2. STM32 `uart_debug_tx_count` 증가를 확인한다.
3. 연결된 ESP32에서 `UART_RX id=0x13`, `MESH_TX id=0x13`을 확인한다.
4. 다른 ESP32 각각의 `MESH_RX source=... id=0x13`을 확인한다.

예전부터 누적된 invalid/noop/self 카운터를 버튼 성공으로 세지 않는다.
사용자가 누른 시점의 새 이벤트와 상대 수신을 연결해서 판단한다.

## 실제 버튼 관찰 결과 (2026-08-28 11:57:43 KST부터 32초)

- 사용자가 버튼을 누르는 동안 STM32 ST-LINK VCP에서 `0x13`을 **4바이트** 수신했다.
- 상대 시각: 20.38s, 21.88s, 23.42s, 25.60s. 각 기록이 한 바이트였다.
- ESP32 세 대 모두 `event_ready=1`이었다.
- 그 관찰 구간에 ESP32 UART valid 수신 수는 변화하지 않았다:
  76 `0→0`, B6 `0→0`, D6 `299→299`.
- Mesh TX/RX 카운터도 구간 시작·종료 사이 증가하지 않았다.

**확인 완료: 외부 PB6 버튼 → STM32 USART2 송신 → ST-LINK VCP 수신.**
**미확인: STM32 D1의 외부 배선 → ESP32 GPIO18 수신 → Mesh 방송.**
ST-LINK VCP에서 신호가 관측됐으므로 USART2가 ST-LINK 쪽에 연결된 것은 확인됐다.
외부 D1에 동시에 연결됐는지는 이 관측만으로 알 수 없다. SB62/SB63 상태와
ESP32 GPIO18·공통 GND 배선을 확인해야 한다. 임의로 납땜 연결을 변경하지 않았다.
