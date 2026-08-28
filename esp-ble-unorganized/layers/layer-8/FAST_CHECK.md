# 빠른 버튼 → Mesh 검증

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-8
bash fast-check.sh
```

설치된 Python/pyserial 환경을 재사용한다. ESP-IDF 환경 로딩이나 빌드가 필요 없다.
한 번 실행하면 포트를 계속 열어 둔다. 다른 시리얼 모니터는 먼저 종료한다.
`READY`가 나오면 외부 버튼을 누른다. 각 단계 도착은 즉시 출력하고,
첫 STM32 `0x13`부터 3초 뒤 카운터를 다시 읽어 결과를 출력한다.
종료 시점 상태 응답이 없으면 최대 1초 더 기다린 뒤 경고한다.
다음 `READY`에서 다시 누르면 된다. Ctrl-C로 종료하며 모든 포트를 해제한다.

## 현재 테스트 배선

- 외부 버튼: STM32 D10/PB6 → 버튼 → GND.
- 송신: STM32 **D8/PA9/USART1_TX** → **D6의 GPIO18**.
- STM32 GND ↔ D6 GND. 반대 방향 D2/PA10 ↔ GPIO17은 이 단방향 시험에서 분리.
- STM32, D6, 76, B6 모두 USB 연결. 76/B6는 UART 선 없이 Mesh 수신 관찰.
- ESP32의 글자 RX/TX 핀이 아니라 숫자 GPIO18/17을 사용한다.

현재 STM32 펌웨어는 USART1으로 실제 송신한 뒤, 성공한 바이트를 USART2의
ST-LINK USB에 진단용으로 복사한다. 이 도구는 그 바이너리 `0x13`을 읽는다.
진단 복사 실패는 원래 송신을 재시도하거나 송신 카운터를 바꾸지 않는다.
USART2의 D0/D1은 ESP32 연결에 사용하지 않는다. USB 복사본만으로 D8 헤더의
전기적 출력이나 ESP32 수신을 입증하는 것은 아니므로 상대 로그도 함께 확인한다.

## 판정

`PASS_OBSERVED`는 같은 관찰 창에서 아래 건수가 모두 같고,
새 UART 잡음/오류 및 설정 변화가 없었다는 뜻이다.

1. STM32 USB 송신 `0x13`.
2. D6 `UART_RX id=0x13 result=queued`.
3. D6 `MESH_TX id=0x13 api=accepted`.
4. 76 및 B6 각각의 `MESH_RX source=<D6 주소> id=0x13 result=queued`.

`INCOMPLETE`는 도착하지 않은 단계, `WARN`은 다른 ID·중복·카운터 불일치·
UART 오류·상태 응답 누락 등을 뜻한다. 송신이 없으면 10초 뒤 `WAIT`를 한 번
출력하고 계속 대기한다. D10 단선 여부를 소프트웨어만으로 확정하지는 않는다.

빠르게 여러 번 누르면 한 창 안에서 건수를 합산한다. 판정 직전의 추가 누름은
경고가 될 수 있으므로, 가장 명확한 시험은 **한 번 누르고 다음 READY까지 기다리기**다.
오래된 누적 카운터 자체는 실패 원인이 아니며 시험 전후 증가량을 비교한다.

현재 프로토콜에는 고유 sequence/ACK가 없다. PASS는 시간·ID·출처·건수의
일치 관찰이지 개별 패킷의 인과관계/재전송/다중 홉 증명이 아니다.
상대 ESP32의 UART 출력 또는 상대 STM32의 실제 수신까지 보장하지 않는다.
USB 로그 도착 순서가 뒤바뀌면 보수적으로 경고/누락으로 판정할 수 있다.

## 옵션과 안전 범위

```bash
bash fast-check.sh --duration 5       # 5초만 확인하고 포트 해제
bash fast-check.sh --source 76        # 송신 보드를 바꾼 경우; 나머지 두 보드가 수신자
bash fast-check.sh --json             # 기계 처리용 JSON Lines 출력
python3 -m unittest discover -s host-tests -p test_fast_check.py -v
```

포트 번호 대신 USB serial/MAC으로 현재 네 보드를 식별한다.
다른 하드웨어를 사용하면 tools/fast_check.py의 IDENTITIES를 실제 장치에 맞춰야 한다.
누락/사용 중/분리된 포트는 오류로 종료한다. 재연결한 뒤 다시 실행한다.
이 프로그램을 중복 실행하지 않는다. 열린 세션 하나를 계속 사용한다.

ESP32에 쓰는 명령은 읽기 전용 `status`뿐이고 STM32에는 아무것도 쓰지 않는다.
DTR/RTS 제어 신호를 조작하지 않으며 flash/reset/키 변경/SWD 접근은 하지 않는다.
로그 원문 전체를 저장하지 않고 필요한 이벤트와 카운터만 출력한다.

`--replay <파일>` 또는 `--replay -`는 USB에 접근하지 않는 가짜 로그 검사다.
JSONL 항목은 `{ "at": 1.0, "board": "STM32", "hex": "13" }`,
`{ "at": 1.1, "board": "D6", "line": "UART_RX id=0x13 result=queued" }`,
시간만 진행할 때는 `{ "at": 4.2 }` 형식이다. 실제 형식의 status 및 QUEUE
스냅샷도 시작 전/판정 창 종료 후에 필요하다. 회귀 테스트가 완전한 예시다.
