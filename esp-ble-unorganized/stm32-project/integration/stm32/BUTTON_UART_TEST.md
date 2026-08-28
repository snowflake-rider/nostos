# 버튼 → USART1 → ESP32 시험

## 최신 설정: 버튼 4개 (2026-08-28)

사용자가 확인한 실제 배선에 맞춰 IOC 라벨과 C 메시지 매핑을 동기화했다.
물리 핀은 기존과 같으며 버튼 1/2의 UP/DOWN 메시지만 서로 교정했다.

| 버튼 | 보드 핀 | MCU 핀 | IOC 라벨 | 전송 바이트 |
| --- | --- | --- | --- | --- |
| 1: 속도 UP | D4 | PB5 | BTN1_SPEED_UP | `0x11` |
| 2: 속도 DOWN | D6 | PB10 | BTN2_SPEED_DOWN | `0x10` |
| 3: 안전운전 알림 | D7 | PA8 | BTN3_SAFETY | `0x12` |
| 4: 정지 요청 | D9 | PC7 | BTN4_STOP | `0x13` |

**최종 관찰 결과:** 버튼 1·2·3은 1차 시험에서 각 1회, 버튼 4는 단독 재시험에서
사용자가 두 번 눌렀다고 확인했고 각 단계에서 2회씩 관찰됐다. 두 시험을 합쳐
네 종류 모두 STM32 → D6 UART → 76 Mesh 수신까지 확인했다. 1차에서 미관찰된
버튼 4 입력의 원인은 확정하지 않았으며, 1차 시험 전체를 4/4 성공으로 바꾸지 않는다.

- 네 핀 모두 **GPIO Input + Pull-up**, LOW가 눌림이다. 기존 30ms 폴링
  디바운스를 유지하며 EXTI 인터럽트를 새로 활성화하지 않았다.
- IOC와 `Core/Inc/main.h`, `Core/Src/main.c`를 직접 동기화했다.
  CubeMX 전체 코드 재생성은 실행하지 않았다.
- D10/PB6 보조 시험 버튼과 USART1 D8/PA9 TX, D2/PA10 RX,
  USART2 ST-LINK 진단 경로는 유지했다.
- ASan/UBSan 호스트 테스트 **3/3 PASS**. 네 버튼의 29/30ms 경계,
  길게 누름, 놓기, 다시 누름 및 정확한 UART 바이트를 확인했다.
- Debug 빌드 성공. 대상 ST-LINK `066DFF485277504867161930`의 기존
  Flash 512KB를 백업한 뒤 새 ELF 설치·download verify·reset 성공.
- STM32 reset 후 D6 수신 버퍼가 72바이트에 머물러 관찰을 중단하고
  D6만 재부팅했다. 이후 buffered=0 및 기존 Mesh 주소·키 인덱스·그룹 유지 확인.
  ESP32 Flash/NVS/키는 쓰지 않았다. 수신 정체 원인을 수정한 것은 아니다.
- **실물 1차 결과: 버튼 1·2·3 전달 확인, 버튼 4 미관찰.** 사용자 입력 완료
  확인 후 300초 관찰 로그를 집계했다. STM32 USB 송신 복사본, D6 UART 수신,
  D6 Mesh API 수락, 76 Mesh 수신에서 `0x11`, `0x10`, `0x12`가 각각 1건씩이다.
  `0x13`은 STM32 송신 단계부터 0건이다. 버튼 4 고장이나 배선 원인을 확정하지 않는다.
  STM32 → D6(0x0005) → 76(0x0003)만 관찰했으며 B6는 제외했다.
- D6 UART valid=0→3, noop/invalid/hw_errors=0 유지, buffered=0.
  D6에서 `No outbound bearer found` 경고 3건이 있었지만 목표 세 메시지는
  76에서 실제 수신됐다. 경고가 없는 시험이었다고 해석하지 않는다.
- 버튼 4(D9/PC7) 단독 재시험: 사용자 확인 “두번눌렀어 4번”. `0x13`이
  STM32 USB trace, D6 UART_RX queued, D6 MESH_TX accepted,
  76 MESH_RX(source=0x0005) queued에서 **각각 2건씩** 관찰됐다.
  재시험 중 펌웨어·설정 변경이나 보드 reset은 하지 않았다.
- 재시험의 목표 버튼 입력 직전→종료 D6 UART valid=5→7,
  noop=3/invalid=0/hw_errors=0 유지, buffered=0. 그보다 앞선 대기 구간의
  별도 `0x20` 수신 2건과 noop 증가 3건은 버튼 성공 횟수에서 제외했다.
  목표 두 이벤트에도 `No outbound bearer found` 경고가 있었으나 76의 실제
  수신 2건을 확인했다. 경고 원인을 해결했다는 뜻은 아니다.
- 재시험 자료는 `button4-retry/`에 보존한다. 관찰 종료 후 세 포트를 닫았다.
  LED·음성 출력, B6, 다중 홉 Relay, 장시간 안정성은 이번 판정 범위 밖이다.

이번 백업·설치·관찰 자료: `build/four-button-test.9qb2nbf_/`.
버튼 로그는 `after-d6-reboot/raw.jsonl` 및 `console.log`에 저장한다.
아래 단일 버튼 기록의 ELF 해시·RAM 주소·초기 실패 결과는 당시 기록이다.

## 이전 단일 버튼 설정: USART1 D8/PA9 (2026-08-28 변경)

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
