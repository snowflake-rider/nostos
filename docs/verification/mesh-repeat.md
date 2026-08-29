# 반복 송수신 / Relay OFF–ON–OFF 검사

[검증 안내](index.md) · [스크립트](../../tools/hardware/mesh_repeat.py)

**처음 실행한다면 [짧은 단계별 안내](../../tests/mesh/README.md)를 보세요.** 이 문서는 고급 옵션과 판정 근거입니다.

ESP32-S3 세 대의 **기존 C000 Generic OnOff** 명령과 수신 로그를 반복 대조한다.
펌웨어를 수정하거나 설치하지 않는다. C001 이벤트·STM32 버튼·UART 경로 시험이 아니다.
2026-08-28 확인된 펌웨어에는 C001 USB 송신 명령이 없으며, C000 설정도 별도로 필요하다.

## 1. 준비

1. [Mesh Console](../../apps/mesh-console/README.md)을 실행하고 웹 화면에서 D6·76·B6를 연결한다.
2. Console 웹 화면은 열어 둔다. 스크립트는 **이미 연결된 Console의 API/로그 스트림**만 사용한다.
   시리얼 포트를 따로 열지 않고 다른 모니터를 종료하지도 않는다.
3. 기존 C001 키·설정은 유지한다. nRF Mesh에서 시험에 쓸 같은 AppKey를:
   - 송신 노드의 **Generic OnOff Client**에 Bind하고 Publication을 **C000**, TTL **7**, period **0**으로 설정.
   - 수신 노드의 **Generic OnOff Server**에 Bind하고 Subscription에 **C000**을 추가.
   - 송신자를 바꿔 가며 시험하려면 세 노드 모두 Client와 Server를 설정.
4. 아래 사전 점검을 실행한다. `C000_SOURCE_NOT_READY`면 송신하지 않는다.
   `onoff_ready`는 Client 준비만 보여 준다. 수신 Server의 Bind/Subscription은 앱에서 따로 확인해야 한다.

저장소 루트에서:

```sh
bash tools/hardware/run_mesh_repeat.sh check
```

`check`는 HTTP GET만 사용하며 키 값은 읽지 않는다. 현재 C001 준비 상태·장치 식별·주소·
최신 status도 확인한다. NetKey/AppKey **인덱스는 키 값 자체의 동일성 증거가 아니다.**
앱의 가상 서버(`mode != live`)에는 실물 시험을 실행하지 않는다.

실행기는 Console의 `.venv`를 재사용한다. 없다면 `MESH_REPEAT_PYTHON`으로 Python 3.11+
환경을 지정한다. `run`의 WebSocket은 Console과 같은 `websockets==17.1`을 사용한다.
`check`, `compare`, 모의 검사에는 WebSocket 패키지가 필요 없다. 자동 설치하지 않는다.

## 2. 20회 / 1시간 / 계속 반복

```sh
# 5초 간격, D6가 ON/OFF를 번갈아 20회 송신. 76·B6의 수신을 검사.
bash tools/hardware/run_mesh_repeat.sh run --send --count 20

# 1시간 이내 반복. 중간 종료 가능.
bash tools/hardware/run_mesh_repeat.sh run --send --count 0 --duration 3600

# Ctrl-C 또는 로그 용량 제한까지 계속 반복.
bash tools/hardware/run_mesh_repeat.sh run --send --count 0

# 송신자를 76으로 변경.
bash tools/hardware/run_mesh_repeat.sh run --send --source 76 --peers D6 B6 --count 20
```

`--send` 없이는 송신하지 않는다. 명령은 `on-unack`/`off-unack` 두 가지뿐이다.
기본 수신 창은 3초, 송신 간격은 5초이며, 한 회차를 마친 뒤 다음 것을 보낸다.
정확한 무선 지연·ACK·고유 packet sequence를 측정하는 시험이 아니다.

- `RECEIVE_MATCH_OBSERVED`: 해당 명령의 관찰 창에서 각 수신 노드에 같은 source/value가 정확히 한 번 보임.
- `MISSING_RECEIVE`: 일부 또는 모든 수신 로그가 없음. 반복은 계속하며 누락 회차를 누적.
- `AMBIGUOUS`: 중복·다른 source/value·다른 제어 명령·늦은 수신 등으로 대응이 불확실. 즉시 중단.
- 끊김·상태 변경·로그 유실·오류·서버 재시작: 자동 재연결/재전송 없이 중단.
- 명령 제출 성공은 USB write일 뿐이다. 수신 로그 없이는 성공으로 판정하지 않는다.
- 다른 앱/Console 탭에서 ON/OFF·출력 설정 등을 동시에 조작하지 않는다.
  같은 checkout의 반복 실행기는 잠금으로 중복 실행을 막지만, 다른 도구의 독점 제어까지 보장하지 않는다.

종료 코드 0은 **실행이 정상 종료됨**을 뜻한다. 누락 없는 무선 성공을 뜻하지 않는다.
`summary.json`의 matched/missing/ambiguous와 `trials.csv`를 함께 본다. 오류는 종료 코드 2다.
Ctrl-C/시간 제한이 회차 도중 발생하면 해당 회차는 불완전/AMBIGUOUS로 남는다.

## 3. 실제 중계 효과 비교

세 노드가 가까이 있으면 직접 수신과 중계를 구분할 수 없다. 이 절에서는:

- 송신 **D6**, 중계 **B6**, 최종 수신 **76**으로 고정.
- D6↔76의 직접 경로를 거리/차폐로 제한하고, B6를 양쪽에 닿는 위치에 배치.
- **D6와 76의 Relay를 끄고**, 시험 대상 B6 이외의 중계기·우회 경로를 제외.
- nRF Mesh에서 설정 후 실제로 다시 읽어 확인. 다른 송신 앱은 닫고 위치·출력·키·TTL을 고정.
- 세 보드의 USB 로그를 동시에 수집할 수 있어야 한다. 긴 USB 케이블/허브 배치의 제약은 사용자가 준비한다.

스크립트는 Relay를 변경하지 않는다. `--confirm-isolated-topology`는 위 조건을 직접 확인했다는
**사용자 선언**이지 자동 검증 결과가 아니다. `--conditions`에는 거리·배치·출력·TTL을 기록하되 키를 적지 않는다.

같은 `--conditions` 내용과 같은 회차 수로 아래 순서대로 실행한다. 각 출력 디렉터리는 새 경로여야 한다.

```sh
# nRF Mesh에서 B6 Relay OFF, 재조회 확인 후 실행.
bash tools/hardware/run_mesh_repeat.sh run --send --source D6 --peers 76 --relay B6 \
  --phase relay-off --confirm-isolated-topology --conditions '배치 A, 위치/출력 고정, TTL 7, 다른 중계 없음' \
  --count 20 --out build/hardware-results/relay-off-1

# 위치를 바꾸지 않고 B6만 Relay ON(3 transmissions / 20 ms), 재조회 후 실행.
bash tools/hardware/run_mesh_repeat.sh run --send --source D6 --peers 76 --relay B6 \
  --phase relay-on --confirm-isolated-topology --conditions '배치 A, 위치/출력 고정, TTL 7, 다른 중계 없음' \
  --count 20 --out build/hardware-results/relay-on

# 같은 위치에서 B6를 다시 OFF, 재조회 후 실행.
bash tools/hardware/run_mesh_repeat.sh run --send --source D6 --peers 76 --relay B6 \
  --phase relay-off --confirm-isolated-topology --conditions '배치 A, 위치/출력 고정, TTL 7, 다른 중계 없음' \
  --count 20 --out build/hardware-results/relay-off-2

bash tools/hardware/run_mesh_repeat.sh compare \
  build/hardware-results/relay-off-1/summary.json \
  build/hardware-results/relay-on/summary.json \
  build/hardware-results/relay-off-2/summary.json
```

각 구간 최소 5회, 동일 조건/회차 수, 오류·중복 없음, OFF 두 구간에서 최종 수신 0건,
ON에서 모든 회차 수신이면 `RELAY_EFFECT_OBSERVED`다. 하나라도 만족하지 않으면 `INCONCLUSIVE`다.
OFF에서 수신되면 직접/우회 경로가 남은 것이므로 그 배치에서 중계 효과를 분리할 수 없다.

이는 통제 조건에 의존한 **중계 효과 관찰**이며 RF 패킷의 정확한 홉 경로를 증명하지 않는다.
실제 packet ID·경로·통계적 전파 성능 검증에는 별도의 펌웨어 진단/스니퍼가 필요하다.
시험 후 Relay 설정을 원래대로 돌릴지는 사용자가 nRF Mesh에서 결정한다. 자동 복원하지 않는다.

## 증거와 안전 범위

매 실행은 Git에서 제외되는 `build/hardware-results/mesh-repeat-*` 또는 `--out`의 새 디렉터리에 저장한다.

- `events.jsonl`: 시작 설정, 송신 의도, 관련 명령/OnOff 수신 로그, 회차 판정. 알 수 없는 로그나 키는 저장하지 않는다.
- `trials.csv`: 회차·값·수신 노드별 건수·판정·문제.
- `summary.json`: 누적 카운터, 설정/조건, 종료 이유, 해석 한계. 기존 실행을 덮어쓰지 않는다.

메모리는 회차·노드 수에 비례하며 전체 로그를 쌓아 두지 않는다. 로그는 기본 약 50 MiB에서
중단한다(`--max-log-mb`, 최대 1024). 마지막 이벤트/요약 때문에 제한을 소폭 넘을 수 있다.
이 제한은 합성 부하/오류로 디스크가 계속 증가하는 것을 막기 위한 것이며, 중단 없이 무한 보관하는 기능은 아니다.

Ctrl-C 시 스트림만 닫는다. 기존 Console/포트는 강제 해제하지 않는다. 웹 UI도 닫혀 스크립트가
마지막 구독자였으면 기존 서버 정책에 따라 약 2초 뒤 포트가 해제된다.
ON/OFF 최종 상태는 그대로 남으며 자동 OFF/복원 명령을 보내지 않는다.

현재 펌웨어의 `status relay=...`는 캐시라 갱신이 늦을 수 있다. `retransmit=0`도 **Vendor Publication 값**이지
Relay 재전송 설정이 아니다. 이 두 필드로 Relay 상태·간격을 확정하지 않는다.

## 도구 회귀 검사

```sh
python3 -m unittest discover -s tests/integration -p test_mesh_repeat.py -v
```

모의 자료로 판정·오류 경로를 확인한다. 실제 무선 전달이나 중계 성공의 증거가 아니다.

### 2026-08-28 구현 확인

- 새 모의 검사 24/24 통과: 반복 성공/누락, 과거 로그 제외, 중복/오염, 상태 변경,
  연결 끊김, Ctrl-C, 시간 제한, 미준비 시 무송신, OFF/ON/OFF 비교.
- `bash tools/test-host.sh` 전체 호스트 회귀 통과. MCU Flash나 무선 시험은 포함하지 않는다.
- 실제 `check`는 D6의 `C000_SOURCE_NOT_READY`로 종료 코드 2를 반환했다. 세 노드의
  C001 `event_ready=1`은 유지됐지만 `onoff_ready=0`이었다. 실패를 숨기거나 송신하지 않았다.
- 실제 Console WebSocket의 live snapshot 수신·정상 종료를 확인했다. 명령 POST 없음.
- **실제 반복 ON/OFF 송신·다중 홉·장시간 RF 안정성은 미검증**이다. C000 설정 후 위 절차를 실행한다.
