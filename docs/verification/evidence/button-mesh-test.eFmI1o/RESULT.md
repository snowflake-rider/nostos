# 연결된 ESP32 두 대의 UART → Mesh 실기 관찰

## 결과

STM32에서 나온 `0x13` 세 건 모두 D6 UART 수신 및 76 Mesh 수신까지 대응됨.
사용자가 실제로 버튼을 세 번 눌렀다고 확인함. 이번 세 번의 누름에 대해 각 단계에서 세 건씩 관찰되어, 누름 횟수와 수신 횟수가 일치함.

| 관찰 단계 | 횟수 |
| --- | ---: |
| STM32 ST-LINK USB 송신 복사본 `13` | 3 |
| D6 UART_RX `id=0x13 result=queued` | 3 |
| D6 MESH_TX `id=0x13 source=0x0005 api=accepted` | 3 |
| 76 MESH_RX `source=0x0005 id=0x13 result=queued` | 3 |

## 시간 대응

아래는 호스트가 각 USB 로그를 읽은 상대 시각이며, 정밀 무선 지연 측정값이 아님.

| 건 | STM32 송신 복사본 | D6 UART 수신 | 76 Mesh 수신 |
| --- | ---: | ---: | ---: |
| 1 | 16.228초 | 16.232초 | 16.265초 |
| 2 | 16.818초 | 16.821초 | 16.845초 |
| 3 | 17.244초 | 17.247초 | 17.276초 |

## 상태와 범위

- D6 주소 0x0005, 76 주소 0x0003. 두 보드 모두 net 0x0000 / app 0x0001 / pub 0xc001 / sub_C001=1 / event_ready=1 유지.
- 준비 이후 D6 UART valid 0→3, noop/invalid/hw_errors 모두 0 유지, 수신 버퍼 0.
- 76의 UART valid/noop/invalid/hw_errors 모두 0 유지. 이번 목표 신호는 UART가 아니라 Mesh 수신 로그로 확인.
- 다른 source 주소의 배경 Mesh 트래픽은 이번 STM32 전달 실적으로 세지 않음.
- 시험 중 펌웨어 쓰기, 리셋, 핀/IOC/Mesh 설정 변경 없음. 현재 D10/PB6 버튼 설정 그대로 사용.
- B6는 대상에서 제외. 다중 홉 중계 및 LED/소리 출력은 이번 시험 범위가 아님. 물리적 누름 횟수는 사용자의 확인에 근거함.
- 모든 시리얼 포트는 관찰 종료 시 닫음.

원본 증거: `raw.jsonl` (모든 수신 로그 및 status), `console.log` (관련 이벤트 발췌).
