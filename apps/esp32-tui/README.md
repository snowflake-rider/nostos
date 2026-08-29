# ESP32 TUI

D6·76·B6의 설정 요약을 보는 **읽기 전용** 터미널 화면입니다.
사람은 TUI, Codex는 JSON을 사용하며 둘 다 [esp32-scan](../../scripts/README.md)을 호출합니다.

## 실행

저장소 루트에서:

```sh
bash scripts/esp32-tui
```

내부 통신 서버는 **필요하면 자동으로 시작**합니다. 별도 터미널이나 웹 화면은 필요 없습니다.
이미 실행 중인 Mesh Console은 재사용합니다. 보드 없이 둘러보려면 `bash scripts/esp32-tui --demo`.

| 키 | 기능 |
| --- | --- |
| `1` / `2` / `3` | D6 / 76 / B6 선택 |
| `r` | 세 보드 다시 조회 |
| `h` | OnOff 설정 안내 보기/닫기 |
| `↑` / `↓`, `PgUp` / `PgDn` | 상세 내용 스크롤 |
| `q`, `Esc`, `Ctrl+C` | 종료 및 진행 중인 조회 취소 |

첫 화면에서 한 번 조회하며, 이후에는 `r`로만 조회합니다. 좁은 터미널은 보드 목록을 접습니다.
이 화면의 값은 **마지막 관찰값**이지 현재 USB 연결 표시가 아닙니다.
조회 실패 시 이전 값임을 표시하고, 읽지 못한 값은 `조회 불가`로 남깁니다.

## Codex / 스크립트

```sh
bash scripts/esp32-tui --json
bash scripts/esp32-tui --json --usb-only
```

`--json`을 첫 옵션으로 주면 같은 자동 준비 기능으로 JSON을 출력합니다(TUI 의존성 불필요).
TUI는 `--port 8787`, `--no-connect`도 지원합니다. 후자는 서버를 새로 시작하지 않고 이미 연결된 보드만 조회합니다.
`--demo`, `--help`, JSON의 `--usb-only`도 서버를 시작하거나 USB 포트를 열지 않습니다.

자동 시작한 서버는 활성 조회·웹 연결 없이 약 10초 지나면 종료됩니다.
TUI 화면은 마지막 관찰값을 유지하고, 다음 `r`에서 서버를 다시 준비합니다.
웹 화면을 사용 중이거나 사용자가 따로 시작한 서버는 종료하지 않습니다.

## 설치와 검사

다른 Mac에서 최초 한 번, Node.js 22 이상으로:

```sh
cd apps/esp32-tui
npm ci
npm run typecheck
npm test
```

OpenTUI와 전용 Bun은 이 폴더의 `node_modules`에만 설치합니다. 시스템 Bun은 바꾸지 않습니다.
상태 조회에는 [Mesh Console Python 환경](../mesh-console/README.md#실행)이 필요합니다.
이 Mac에는 이미 준비되어 있습니다. 다른 Mac에서는 해당 안내로 먼저 준비하세요. TUI가 자동 설치하지는 않습니다.
[create-tui](https://github.com/msmps/create-tui)의 Core 템플릿 기반이며,
[OpenTUI](https://github.com/anomalyco/opentui) 가상 화면·키 입력 검사와 모의 프로세스 검사를 포함합니다.

## 범위

설정·Relay·ON/OFF·Flash·reset은 변경하지 않습니다. 키 원문이나 임의 로그를 표시하지 않습니다.
현재 펌웨어가 노출하지 않는 실제 모델별 Bind, C000 발행 세부값, Server 구독,
현재 Relay 값은 TUI에서도 읽을 수 없습니다. 새 STATUS도 내부 설정의 최신 재조회를 보장하지 않습니다.
조회 종료 시 USB 해제는 기존 Console 정책을 따르며 다른 사용자의 연결을 강제로 끊지 않습니다.

| 오류 | 확인할 것 |
| --- | --- |
| `CONSOLE_ENVIRONMENT_MISSING` / `CONSOLE_START_FAILED` | Console `.venv` 및 Python 의존성 준비 여부 |
| `CONSOLE_PORT_NOT_COMPATIBLE` / `CONSOLE_NOT_LIVE` | 같은 포트에 다른 앱·가상 서버가 실행 중인지 확인. 임의 종료하지 않음 |
| `CONSOLE_PROBE_FAILED` / `CONSOLE_START_TIMEOUT` | 서버 응답 지연·시작 실패. 잠시 뒤 `r`, 계속되면 수동 서버로 오류 확인 |
| `USB_IN_USE_BY_ANOTHER_PROCESS` | 해당 보드를 사용하는 시리얼 모니터 확인 |

송수신 검증은 별도의 [단계별 시험](../../tests/mesh/README.md)입니다. TUI 검사는 무선 성공 증거가 아닙니다.
