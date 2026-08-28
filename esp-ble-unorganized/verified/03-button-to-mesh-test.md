# 03. 버튼 → STM32 → D6 → 76 — 3/3

검증일: 2026-08-28 (KST). 사용자가 요청한 **현재 연결된 ESP32 두 대만** 확인했다. B6는 제외했다.

## 결과

사용자가 버튼을 **세 번 눌렀다고 확인**했고, 각 단계에서 같은 정지 요청 `0x13`이 **세 건씩** 관찰됐다. 이번 세 번의 누름은 누락이나 추가 중복 없이 76의 Mesh 수신까지 이어졌다.

| 단계 | 실제 증거 | 횟수 |
| --- | --- | ---: |
| 사용자 입력 | 사용자 확인: “세번 눌렀어” | 3 |
| STM32 송신 | ST-LINK `USB_TRACE hex=13` | 3 |
| D6 UART 수신 | `UART_RX id=0x13 result=queued` | 3 |
| D6 Mesh API 수락 | `MESH_TX id=0x13 source=0x0005 api=accepted` | 3 |
| 76 Mesh 수신 | `MESH_RX source=0x0005 id=0x13 result=queued` | 3 |

핵심은 마지막 행이다. 송신 API가 수락했다는 로그에 그치지 않고 **상대 보드 76이 D6 주소 `0x0005`의 같은 ID를 받았음**을 확인했다.

## 시험 방법

1. 임시 주기 송신 시험을 끝내고 원래 STM32 펌웨어가 복구된 상태를 사용했다.
2. STM32·D6·76의 USB를 식별값으로 열었다. reset 제어선을 토글하지 않았다.
3. ESP32에는 조회용 `status`만 보냈다. 두 보드의 Mesh 준비와 D6 수신 버퍼 0을 확인했다.
4. 준비 후 사용자에게 D10/PB6 버튼을 눌렀다 떼도록 요청했다.
5. 사용자는 세 번 눌렀고, 세 단계의 실제 수신 로그를 시간·source·ID로 대응시켰다.
6. 첫 대상 이벤트 후 6초까지 관찰하고 모든 포트를 닫았다.

이 버튼 시험 중에는 Flash 쓰기, 보드 reset, 핀·IOC·Mesh 설정 변경이 없었다.

## 시간 대응

| 건 | STM32 송신 복사본 | D6 UART 수신 | 76 Mesh 수신 |
| --- | ---: | ---: | ---: |
| 1 | 16.228초 | 16.232초 | 16.265초 |
| 2 | 16.818초 | 16.821초 | 16.845초 |
| 3 | 17.244초 | 17.247초 | 17.276초 |

모두 관찰 시작 기준의 **호스트 로그 수집 시각**이다. MCU 간 시계 동기화나 정밀 무선 지연 측정은 하지 않았다. UART 데이터에는 시험별 순번이 없으므로 이번 대응은 사용자 입력 횟수, 시간, source, ID, 카운터에 근거한 관찰이다.

## 오류·다른 트래픽 구분

- D6 UART: valid **0→3**, noop·invalid·hw_errors **0 유지**, buffered **0 유지**.
- 76 UART: valid·noop·invalid·hw_errors **0 유지**. 목표 데이터는 76의 UART가 아니라 **Mesh 수신 로그**에서 확인했다.
- 양쪽 Mesh 주소·키 인덱스·그룹 설정은 관찰 중 유지됐다.
- 다른 source 주소의 배경 Mesh 트래픽은 이번 성공 횟수에 포함하지 않았다.
- 관찰 구간의 세 건에 대해 누락·추가 중복이 없었다. 장시간·다수 노드에서 같은 보장을 한 것은 아니다.

## 코드 경로

1. [button.c](../stm32-project/integration/stm32/MyApp/hw/button.c): D10/PB6 눌림 전이를 `MSG_STOP_REQUEST`로 만든다.
2. [app.c](../stm32-project/integration/stm32/MyApp/ap/app.c): 버튼 메시지를 로컬 publish 경로에 넘긴다.
3. [uart_service.c](../stm32-project/integration/stm32/MyApp/service/uart_service.c): USART1로 한 바이트 송신하고, HAL 성공 시 USART2에 진단 복사본을 출력한다.
4. [bridge_runtime.c](../layers/layer-8/main/bridge_runtime.c): D6 UART 입력을 Mesh로 넘기고, 76의 Mesh 수신 콜백에서 source·ID·queued 결과를 기록한다.

원본 로그와 보존 발췌는 [05. 증거](05-evidence.md), Relay·출력 등 이번 범위 밖 항목은 [04. 한계](04-recovery-and-limits.md)를 참고한다.
