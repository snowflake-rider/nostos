# Testing — 메시지·보드·출력 검증

## 지금 할 시험: 8종 메시지 Group Delivery

**STM32 USART1 → 송신 ESP32 D6 → Bluetooth Mesh 그룹 C001 → 수신 ESP32 76·B6**를 메시지마다 확인한다. 센서/버튼 입력을 대신하는 시험 명령을 쓰지만, USART1 송신과 ESP32 UART/Mesh 전달은 실제 코드와 보드를 사용한다. Relay 시험은 별도다.

```sh
# 1. 보드 없이 판정기·STM32 UART·Mesh payload/큐 회귀 검사
bash testing/run_message_host_tests.sh

# 2. 연결·그룹·Relay OFF·STM32 시험 명령 준비 확인 (이벤트 송신 없음)
bash testing/run_message_broadcast.sh

# 3. 8개 ID 각각 1회 송신하여 양쪽 수신 확인
bash testing/run_message_broadcast.sh --send

# 4. 같은 8개 ID를 세 차례 반복 (총 24회)
bash testing/run_message_broadcast.sh --send --repeat 3
```

위 명령은 프로젝트 루트에서 실행한다. 다른 위치에서는 스크립트의 절대 경로를 사용한다. **시험 명령이 추가된 STM32 `BUTTON_OUTPUT_TEST=ON` 이미지가 필요**하다. 스크립트는 Flash/reset을 자동 실행하지 않는다. 준비 및 설치 절차는 [메시지 시험 상세](MESSAGE_BROADCAST.md)를 따른다.

수신 보드에 STM32나 다른 장치가 연결되어 있으면 실제 이벤트 ID가 UART로 출력된다. 특히 낙차/SOS 메시지를 실제 경보 시스템에 연결한 상태로 시험하지 않는다. 시험 중 버튼·센서의 별도 입력과 다른 시리얼 모니터를 중지한다.

## 도구와 기록

최근 준비 기록: [2026-08-28 구현·호스트 검사·MCU 빌드 결과](results/message-broadcast-5e8yqnqu/RESULT.md). 이 실행에서는 USB 장치가 연결 목록에서 사라져 **Flash와 8종 실물 수신은 미실행**이다.

| 항목 | 용도 |
| --- | --- |
| [message_broadcast.py](message_broadcast.py) | USB 식별, 준비 확인, 8종 송신, ID/source/건수 판정 |
| [run_message_broadcast.sh](run_message_broadcast.sh) | 설치된 pyserial Python으로 실행 |
| [run_message_host_tests.sh](run_message_host_tests.sh) | Python 판정기 + 기존 STM32·Layer 8 호스트 회귀 검사 |
| [MESSAGE_BROADCAST.md](MESSAGE_BROADCAST.md) | 단계별 실행, 판정 기준, 설치와 복구 경계 |
| [AUDIO_TESTING.md](AUDIO_TESTING.md) | 기존 버튼·RGB·버저·VS1003B 진단 안내 원문 |
| [codec_diag.py](codec_diag.py) | 기존 오디오 USB 관찰·진단 명령 |
| [run_stm32_host_tests.sh](run_stm32_host_tests.sh) | 기존 STM32 전용 검사 진입점 |
| [checklist.md](checklist.md) | 기존 사용자 관찰·실물 시험 이력 |
| [results/](results/) | 실행별 원본 로그·JSON·결과표. 기존 기록은 이동/삭제하지 않음 |

`tests/`의 가짜 로그 검사는 판정 코드 테스트일 뿐 실제 Mesh 수신 증거가 아니다. 실제 시험 결과는 각 실행의 `RESULT.md`와 `raw.jsonl`에서 확인한다. [메시지 형식 설명](../message-protocol/README.md)도 함께 참고한다.
