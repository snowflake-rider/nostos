# Mesh Console

Mac USB에 연결된 Layer 8 ESP32 세 대를 보는 로컬 웹 앱입니다.
기존 펌웨어·Mesh 설정을 변경하지 않습니다.

## 실행

```bash
cd /Users/kafka/Workspace_AI/esp-ble/apps/mesh-console
npm start
```

브라우저에서 **http://127.0.0.1:8787**을 엽니다. 종료는 서버 터미널에서 `Ctrl+C`입니다.
Node.js 22.12+ 또는 24/26, Python 3.11+가 필요합니다. 처음 실행할 때 이 폴더에만
npm 의존성과 `.venv`를 설치합니다. ESP-IDF Python 환경에는 설치하지 않습니다.

1. 기존 `idf.py monitor`, 시리얼 모니터 등 **같은 보드를 사용하는 프로그램을 먼저 종료**합니다.
2. 왼쪽 D6/B6/76의 **연결**을 누릅니다. 이때만 USB 포트를 엽니다.
3. `콘솔 확인 중`이 `연결됨`으로 바뀌는지 확인합니다. 연결되면 `status`를 5초마다 요청합니다.
4. 로그를 보고, 필요할 때 오른쪽에서 명령을 보냅니다.
5. 작업이 끝나면 각 보드의 **해제**를 누릅니다.

서버 시작/새로고침은 USB 목록만 읽습니다. 감지는 펌웨어나 Mesh 동작 확인이 아닙니다.
모든 웹 화면이 2초 이상 끊기면 USB를 해제합니다. 2초 안의 페이지 새로고침은 기존 연결이
유지될 수 있습니다. USB 분리·오류·서버 재시작 후에는 직접 다시 연결해야 합니다.
여러 탭은 같은 서버의 연결을 공유하므로 한 탭의 해제는 다른 탭에도 적용됩니다.

## 제어 범위

| 화면 | 실제 콘솔 명령 | 조건 |
| --- | --- | --- |
| 상태 조회 | `status` | USB 연결됨 |
| ON / OFF, 응답 요청 켬 | `on` / `off` | 최근 15초 이내 상태, `onoff_ready=1` |
| ON / OFF, 응답 요청 끔 | `on-unack` / `off-unack` | 위와 같음 |
| 송신 출력 일반 | `tx-normal` | 최근 Layer 8 상태 |
| 송신 출력 낮음 | `tx-low` | 최근 Layer 8 상태 + 확인 대화상자 |

**ON/OFF는 C000 시험이며 C001 버튼·낙상 이벤트를 주입하지 않습니다.**
`event_ready`는 이벤트 송신 준비, `sub_C001`은 구독 설정을 각각 표시합니다.
AppKey 추가·binding·publication·subscription은 **nRF Mesh 앱**에서 합니다.
`factory-reset`, flash, erase, 임의 콘솔 명령, STM32 제어는 제공하지 않습니다.

`USB에 전달됨`은 serial write 완료만 뜻합니다. 실제 실행·무선 수신·하드웨어 출력 성공과
구분해서 봐야 합니다. `MESH_TX api=accepted`와 상대의 `MESH_RX`도 별개입니다.
현재 출력 조회 명령이 없으므로 출력은 **마지막 요청**만 표시합니다.

## 로그

- 시간은 Mac이 수신한 시각입니다. 표에서는 ESP 로그 접두어를 접어 메시지를 먼저 보여 줍니다.
  행을 누르면 생략하지 않은 원문과 ESP uptime을 볼 수 있습니다. 저장 파일도 원문입니다.
- 보드/분류/오류 필터와 검색을 함께 적용할 수 있습니다.
- 일시정지는 **화면 표시만** 멈춥니다. USB 수집은 계속됩니다.
- 저장은 현재 필터 결과를 TXT/JSONL로 내려받습니다. 자동 디스크 저장·외부 전송은 없습니다.
- 휴지통은 이 탭의 표시를 지웁니다. 서버 기록이나 장치 상태는 바꾸지 않습니다.
- 서버는 최대 5,000줄 또는 원문 5MiB를 보관합니다. 긴 한 줄은 8KiB에서 생략합니다.
  오래된 로그 생략 수는 하단에 표시합니다. 브라우저도 5,000줄로 제한합니다.

## 보드 식별

포트 이름은 USB 재연결 때 바뀔 수 있어 **USB serial + VID/PID**로 찾습니다.

| 보드 | USB serial | 허용 VID/PID |
| --- | --- | --- |
| D6 | `14:C1:9F:CE:F0:D4` | `303A:1001` |
| B6 | `44:1B:F6:FF:BA:B4` | `303A:1001` |
| 76 | `14:C1:9F:CE:EC:74` | `303A:1001` |

다른 장치를 추가하려면 `server/bridge.py`의 `IDENTITIES`와 `NAMES`를 실제 장치 기준으로
수정해야 합니다. USB serial과 펌웨어 Bluetooth 이름의 끝 두 자리는 서로 다를 수 있습니다.
오래된 Layer 7 펌웨어나 다른 이름의 STATUS는 제어 준비 상태로 인정하지 않습니다.

## 문제 해결

| 증상 | 확인할 것 |
| --- | --- |
| USB 미감지 | 데이터 지원 USB 케이블, 허브, 전원, 위 serial 번호 |
| 포트 열기 오류 | 기존 시리얼 모니터 종료 후 다시 연결. 앱이 다른 프로세스를 강제 종료하지 않음 |
| 콘솔 확인 중이 계속됨 | 올바른 Layer 8 펌웨어인지, 다른 프로그램이 같은 포트를 읽고 있는지 확인 |
| 상태 오래됨 | 최근 STATUS가 15초 이상 없음. 상태 조회를 누르거나 연결 해제 후 다시 연결 |
| ON/OFF만 비활성 | C000 모델의 AppKey binding/publication 등 확인. C001 준비와 별개 |
| 로컬 서버 연결 끊김 | 서버 터미널 확인 후 `npm start`. 앱은 브라우저 스트림만 재시도하고 USB는 자동 연결하지 않음 |
| 8787 포트 사용 중 | 이미 실행 중인 앱이 있는지 확인. 다른 프로세스를 종료하거나 강제 포트 변경하지 않음 |

115200/8N1, POSIX exclusive 요청과 DTR/RTS 변경을 하지 않는 serial 클래스를 사용합니다.
exclusive는 다른 프로그램이 같은 잠금 규칙을 따를 때만 효과가 있으며, OS/USB 드라이버의
제어선 동작까지 보장할 수 없습니다. 실제 포트를 열기 전 다른 모니터를 종료해 주세요.

## 개발과 테스트

```bash
# 정상 앱과 완전히 분리된 fake serial 테스트; 실제 USB를 열지 않음
bash scripts/test.sh

# UI 수정: 아래 서버 + 다른 터미널에서 npm run dev
.venv/bin/python -m uvicorn server.app:app --host 127.0.0.1 --port 8787
npm run dev

# UI 시험 전용 가상 서버: 정상 앱과 다른 포트, 화면에도 테스트 모드 표시
.venv/bin/python -m uvicorn tests.preview:app --host 127.0.0.1 --port 8788
```

가상 서버의 D6/76은 OnOff 준비됨, B6은 C000 미준비 상태를 제공합니다.
시험용 API는 실제 serial opener/scanner를 사용하지 않습니다. 실장치 PASS 증거가 아닙니다.

백엔드는 FastAPI/pyserial, 프런트엔드는 React/Vite/TypeScript입니다.
`requirements-lock.txt`와 `package-lock.json`은 설치 버전을 고정합니다.
서버는 `127.0.0.1`에만 실행하고 Host/Origin과 명령 허용 목록을 검사합니다.
공용 네트워크에 노출하거나 `--host 0.0.0.0`으로 실행하지 마세요.

구현 근거: `../../layers/layer-8/main/`의 실제 console/status 포맷.
화면 기준: `../../docs/superpowers/specs/assets/mesh-console-concept.png`.
시안은 예시 데이터이며 정상 앱은 연결 전 빈 로그·미연결 상태로 시작합니다.
