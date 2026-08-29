# 원격 버튼 오디오 테스트 스위트

버튼 1/2/3 메시지 하나가 아래 경로를 정확히 한 번 통과했는지 판정합니다.

```text
송신 STM32 -> 송신 ESP32 UART_RX -> MESH_TX
           -> 수신 ESP32 MESH_RX -> UART_TX
           -> 수신 STM32 RX/라우팅 -> VS1003B 오디오
```

기존 펌웨어·보드·Mesh 설정을 바꾸지 않는 오프라인 판정기입니다. ESP32 로그와
수신 STM32 카운터, 사람이 확인한 출력 결과를 JSON 하나에 기록합니다.

## 자체 테스트

```sh
bash tests/remote-audio/run.sh
```

검사 항목:

- `speed-down(0x10)`, `speed-up(0x11)`, `stop(0x13)` 정상 경로
- 누락·중복 단계와 잘못된 Mesh source
- `ESP UART_TX -> 같은 ESP UART_RX` 에코/재방송 루프
- 버튼 시험 중 `FALL(0x30)` 또는 `SOS(0x31)` 혼입
- 상대 STM32 RX·remote 카운터가 각각 정확히 1 증가했는지
- 원격 버튼은 오디오만 재생하고 RGB·버저는 바꾸지 않았는지

## 실물 시험 판정

[`example-evidence.json`](example-evidence.json)을 복사하여 실제 로그와 관찰값으로
바꾼 뒤 실행합니다.

```sh
python3 tests/remote-audio/verify.py /path/to/evidence.json
python3 tests/remote-audio/verify.py /path/to/evidence.json --json
```

이벤트 이름은 `speed-up`, `speed-down`, `stop` 중 하나입니다. `audio_status=0`은
VS1003B 드라이버 상태가 정상이라는 뜻이지만 실제 소리가 들렸다는 증거는 아닙니다.
따라서 완전한 `PASS`에는 `audio_heard=true`, `buzzer_heard=false`,
`rgb_changed=false`가 필요합니다.

판정 의미:

- `PASS`: 네트워크 네 단계, STM32 수신·라우팅, 실물 출력이 모두 일치
- `INCOMPLETE`: ESP32 네트워크 경로는 맞지만 상대 STM32 증거가 없음
- `FAIL`: 누락·중복·에코·잘못된 source·안전 이벤트 혼입 또는 출력 불일치

종료 코드는 `PASS=0`, `INCOMPLETE/FAIL=1`, 입력 오류=2입니다.
