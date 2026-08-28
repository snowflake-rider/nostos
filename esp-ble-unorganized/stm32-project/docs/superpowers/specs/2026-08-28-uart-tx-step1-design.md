# UART 연결 시험 — 1단계: STM32 주기 송신

상태: 대화에서 UART 우선 순서 합의 완료. 이 문서 확인 후 프로젝트 생성·빌드 진행.

## 목적과 범위

NUCLEO-F411RE에서 버튼 없이 PA9로 약 1초마다 바이너리 `0x13` 한 바이트를 송신한다.
기존 `integration/stm32` 프로젝트는 보존하고, 별도의 최소 프로젝트로 시험한다.
이번 구현 범위는 STM32 프로젝트 생성과 빌드까지다. Flash와 실제 수신 검증은 별도 단계다.

## 프로젝트와 설정

- 새 경로: `experiments/01-uart-tx/stm32/`
- 프로젝트 이름: `uart_tx_step1`; CubeMX 설정 파일: `uart_tx_step1.ioc`.
- 보드: NUCLEO-F411RE, MCU: STM32F411RET6 / STM32F411RETx, LQFP64.
- PA9(D8): USART1_TX. USART1은 asynchronous, TX only (`UART_MODE_TX`).
- 통신 형식: 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control.
- PA10(D2): 이번 단계에서는 사용하지 않으며 USART1_RX나 버튼 입력으로 설정하지 않는다.
- PA13/PA14: Serial Wire Debug 유지. SysTick과 내부 HSI 16 MHz 사용.
- 버튼, LED, USART2, I2C, SPI, DMA, RTOS, 사용자 UART IRQ는 추가하지 않는다.
- CubeMX에서 CMake 프로젝트를 생성하고, 생성된 핀·클록·UART 초기화를 실제로 확인한다.
- 기존 통합 프로젝트의 MyApp 코드나 센서 초기화 코드를 복사하지 않는다.

## 실행 흐름과 오류 처리

1. HAL, 시스템 클록, GPIO, USART1을 초기화한다.
2. 약 1초를 기다린 후 `HAL_UART_Transmit()`으로 `0x13` 한 바이트를 보낸다.
3. 2번을 반복한다. 문자열 `"13"`, 개행, 로그 문자는 PA9에 보내지 않는다.
4. 송신 timeout은 10 ms로 제한한다. 결과 상태와 성공·실패 횟수를 debugger에서 볼 수 있게 유지한다.
5. 초기화 실패는 `Error_Handler()`에서 정지한다. 송신 실패는 기록하고 다음 주기에 다시 시도한다.

단순 `HAL_Delay(1000)` 반복이므로 간격에는 송신 시간이 더해진다. 정밀한 1초 타이머를 만드는 시험은 아니다.
`HAL_OK`나 송신 성공 횟수는 ESP32 수신 또는 실제 핀 파형의 증거가 아니다.

## 검증과 중단 지점

- `.ioc`와 생성 C 코드 모두 PA9 USART1_TX, TX only, 115200 8-N-1인지 확인한다.
- PA10이 UART 또는 버튼 핀으로 초기화되지 않는지 확인한다.
- ARM용 CMake Debug 빌드와 ELF 생성을 확인하고, 실행 흐름·송신 크기·timeout을 소스에서 검토한다.
- 기존 통합 STM32 소스 및 ESP32/Layer 8 소스는 수정하지 않는다.
- 여기서 멈춘다. 생성·빌드는 보드 Flash, UART 파형, ESP32 수신 성공과 구분해서 보고한다.

## 이후 단계 — 이번에는 구현하지 않음

2단계에서 보드와 펌웨어를 식별하고 Flash를 확인한 뒤 `PA9 → ESP32 RX`, 공통 GND로 실제 수신을 검증한다.
현재 Layer 8의 RX 설정은 GPIO18이며, 사용할 ESP32 보드와 실행 중인 펌웨어에서 재확인한다.
각 보드는 USB로 전원을 공급하고 3V3/5V 전원 레일은 서로 연결하지 않는다.
현재 Layer 8은 수신된 `0x13`을 Mesh로 전달할 수 있으므로, UART 단독 시험 전에 실행 중인 펌웨어와 전달 영향을 확인한다.
UART 수신 성공 후에만 3단계 PA10 버튼 입력과 4단계 Mesh 전달 시험으로 진행한다.
