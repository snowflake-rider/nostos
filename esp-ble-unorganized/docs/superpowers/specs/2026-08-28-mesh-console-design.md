# Mesh Console — 설계 검토안

작성일: 2026-08-28. 상태: 기능 범위·시안 승인 후 `apps/mesh-console/` 구현.

전체 화면 시안: [mesh-console-concept.png](assets/mesh-console-concept.png).
내장 Image Gen으로 생성했으며 1536×1024 화면을 `view_image`로 확인했다.
시안은 예시 데이터이고 실행 중인 웹 앱이나 실장치 검증 결과가 아니다.

## 1. 목표와 범위

Mac USB에 연결된 ESP32-S3 B6·D6·76의 상태와 메시지 로그를 하나의 웹 화면에서 보고,
현재 Layer 8 펌웨어가 이미 제공하는 콘솔 명령을 실행한다.
기존 펌웨어·Mesh 설정·STM32 코드를 변경하지 않는다.

사용자가 승인한 화면 구성:

- 왼쪽: 보드 목록, 명시적인 연결/해제, 최신 Mesh 준비 상태.
- 가운데: 실시간 로그, 보드·이벤트·오류 필터, 검색, 표시 일시정지, 저장.
- 오른쪽: 선택한 보드의 상태 조회, C000 On/Off 시험, 송신 출력 조절.

Mesh Provisioning, AppKey, Publication, Subscription 관리는 기존 nRF Mesh 앱에서 수행한다.
C001 버튼·센서 이벤트 주입, flash/erase/reset, 임의 콘솔 명령, 원격 접근, 클라우드 저장은 제외한다.
웹 앱은 실제 수신한 데이터를 보여 주며 초기 화면에 가짜 텔레메트리를 채우지 않는다.

## 2. 연결 방식

선택: **React + Vite + TypeScript 화면 / Python FastAPI + pyserial 로컬 서버**.

브라우저 Web Serial만 사용하는 대안도 있지만, 이 구성에서는 USB serial에 따른 세 보드 식별,
기존 무리셋 pyserial 방식 재사용, 여러 포트의 수명 관리를 로컬 서버에서 처리한다.
별도 데스크톱 앱 패키징은 이번 범위에 필요하지 않다.

```text
Browser UI ← WebSocket 로그·상태 → localhost Python server
           → HTTP 연결·명령 요청 →        ↓ USB Serial/JTAG
                                  D6 / B6 / 76
```

구현 위치는 새 `apps/mesh-console/`로 한정한다. Python 가상환경과 npm 의존성도 그 안에 둔다.
기존 ESP-IDF Python 환경에 패키지를 설치하지 않고, 다른 앱·firmware의 빌드 설정도 바꾸지 않는다.
배포 빌드에서는 Python 서버가 프런트엔드 정적 파일과 API를 같은 origin에서 제공한다.
개발 모드의 Vite proxy도 같은 `/api` 경로를 사용한다.

## 3. 포트 소유권과 장치 안전

| UI 라벨 | 허용 USB serial |
| --- | --- |
| D6 | `14:C1:9F:CE:F0:D4` |
| B6 | `44:1B:F6:FF:BA:B4` |
| 76 | `14:C1:9F:CE:EC:74` |

- 서버 시작과 목록 새로고침은 포트만 열거한다. 자동 연결하지 않는다.
- 사용자가 해당 보드의 연결 버튼을 눌렀을 때만, USB serial을 다시 찾아 현재 포트를 연다.
- 클라이언트가 임의의 `/dev/...` 경로를 지정할 수 없게 한다. STM32와 알 수 없는 장치는 제어하지 않는다.
- 115200/8N1, timeout, exclusive 접근 및 DTR/RTS 상태 변경을 막는 포트 클래스를 사용한다.
- 기존 프로세스가 점유한 포트는 오류를 표시한다. 프로세스 종료, 강제 탈취, 보드 reset은 하지 않는다.
- 연결 후 `status`를 한 번 요청하고, 연결 중에만 5초 간격으로 갱신한다. UI에서 갱신 간격을 명시한다.
- USB 분리, 읽기 오류, 서버 종료 때 포트를 닫고 상태를 미연결로 바꾼다. 자동 재연결·명령 재전송은 하지 않는다.
- 브라우저 클라이언트가 모두 사라지면 짧은 유예 후 포트를 해제한다. 새로고침 후에도 연결은 다시 명시적으로 선택한다.
- 포트를 열지 않은 상태에서 보이는 것은 USB 장치 감지 결과일 뿐, 실행 펌웨어·Mesh 준비 완료를 뜻하지 않는다.

현재 저장소의 `layers/layer-8/tools/check_uart_diag.py` 무리셋 패턴을 참고하되,
앱이 테스트 도구 모듈 전체에 의존하지 않도록 필요한 포트 클래스를 독립적으로 둔다.

## 4. 허용 명령과 결과 표시

| UI | 콘솔 명령 | 조건·표시 |
| --- | --- | --- |
| 상태 조회 | `status` | 연결된 확인 대상에 허용 |
| ON / OFF + 응답 요청 | `on` / `off` | 최신 `onoff_ready=1`일 때; 그룹 C000 시험임을 표시 |
| ON / OFF + 응답 요청 해제 | `on-unack` / `off-unack` | 위와 동일 |
| 일반 출력 | `tx-normal` | Layer 8 콘솔 확인 후 명시적인 클릭으로 전송 |
| 낮은 출력 | `tx-low` | 통신 거리 감소 경고와 확인 후 전송 |

허용 목록 외의 명령, 여러 줄 입력, `factory-reset`은 서버에서도 거절한다.
빠른 중복 클릭은 제한하고 보드별 write lock으로 명령이 섞이지 않도록 한다.
연결 실패나 포트 write 실패를 자동 재시도하지 않는다.

**명령 상태는 USB에 전달됨 / 전달 실패로만 표시한다.** 콘솔의 성공 응답이 없는 명령을
성공 ACK로 간주하지 않는다. 송신 출력도 조회할 수 없는 경우에는 현재 설정처럼 표시하지 않고
마지막 요청만 표시한다. `MESH_TX api=accepted`와 상대 `MESH_RX`는 별도 로그다.

## 5. 상태 모델과 로그 처리

연결 상태: 미감지 → USB 감지/미연결 → 연결 중 → 콘솔 확인 중 → 연결됨.
오류와 분리는 별도 사유를 표시한다. 오래된 상태는 마지막 수신 시각과 함께 만료 처리한다.

로그의 `LAYER_8_MESH: STATUS`에서 다음 값을 파싱한다:

- node name, primary address, event_ready, net/app index.
- pub, sub_C001, ttl, period, retransmit, relay, onoff_ready.

`event_ready=1`은 송신 준비 표시이며 `sub_C001=1`은 별도로 표시한다.
어느 쪽도 실제 무선 수신 성공을 뜻하지 않는다. 15초 이상 갱신이 없으면 상태를 오래됨으로 표시하고
준비 상태를 요구하는 제어를 비활성화한다. 연결 해제 시 이전 준비 상태를 유지하지 않는다.

로그 레코드: 서버 순번, host 수신 시간, 보드, 방향, 분류, 원문, 선택적 ESP uptime.
host 수신 시간은 노드 간 정확한 송신 시각이나 동기화된 무선 시각이 아니다.
분류는 STATUS / UART RX / MESH TX / MESH RX / UART TX / ERROR / SYSTEM / OTHER다.
알 수 없는 로그도 버리지 않고 OTHER로 표시한다.

청크 사이에서 끊긴 UTF-8과 줄 경계, CRLF, ANSI 색상, 너무 긴 줄을 처리한다.
긴 줄과 버퍼 제한에 따른 생략은 표시한다. 초기 상한은 원문 한 줄 8 KiB, 전체 로그 5000줄이다.
WebSocket 구독자의 대기열에도 상한을 두어 느린 브라우저가 서버 메모리를 무한 증가시키지 않게 한다.
명령·시스템 알림은 장치에서 받은 원문 로그와 명확히 구분한다.

표시 일시정지는 USB 수집을 중단하지 않는다. 로그 지우기는 화면/앱 기록만 지우며 장치 상태는 바꾸지 않는다.
저장은 보이는 필터 결과 또는 전체 보관 로그를 명확히 구분하고, JSONL 또는 텍스트로 브라우저에서 내려받는다.
로그를 디스크에 자동 기록하거나 외부 서비스로 전송하지 않는다.

## 6. 로컬 API와 보안 경계

- `127.0.0.1`에만 바인딩. 허용 Host와 HTTP/WebSocket Origin 검사.
- 연결·해제·명령 요청은 POST. 임의 origin, 미허용 보드·명령, 잘못된 payload는 거절.
- 클라이언트에는 실행 파일·shell·임의 파일·포트 경로를 지정하는 API를 제공하지 않는다.
- API는 USB 감지, 현재 상태, 연결, 해제, 명령 및 스트림 구독으로 제한한다.
- 프런트엔드는 장치 로그를 텍스트로 렌더링하고 HTML로 삽입하지 않는다.
- 여러 탭이 같은 서버에 연결되어도 보드당 serial handle은 하나만 유지한다.
- 개발·시험용 fake serial은 명시적인 테스트 경로에서만 사용하고 실장치 UI와 섞지 않는다.

## 7. 화면과 접근성

차분한 어두운 개발 도구 화면. 보드별 카드 격자 대신 보드 목록·로그 표·선택 항목 패널을 사용한다.
주요 동작만 라임색으로 강조하며 배경에 장식적인 gradient나 glow를 쓰지 않는다.
한국어 sans-serif UI 글꼴과 monospace 로그 글꼴을 분리한다.

설계 시안에는 `화면 시안 · 예시 데이터`를 명시한다. 구현 초기 화면은 실제 연결 전 상태와 빈 로그다.
시안의 예시 주소·카운터·연결 상태를 실제 데이터로 하드코딩하지 않는다.

1536×1024를 기본 데스크톱 비교 화면으로 사용한다. 좁은 화면에서는 보드 선택과 제어를
접을 수 있는 패널로 옮기고 로그를 중심에 둔다. 로컬 서버를 외부에 공개하는 모바일 접근 기능은 만들지 않는다.
키보드 포커스, 버튼 상태, 색상 외 상태 텍스트, reduced motion을 지원한다.

구현 모듈은 App shell / board list / log workspace / controls / connection state / log parser로 나누고,
서버도 serial lifecycle / state parsing / API·stream / identity configuration으로 분리한다.

### 시안에서 추출한 화면 규칙

- 1536×1024에서 상단 약 57px, 하단 약 61px, 왼쪽 약 335px, 오른쪽 약 378px.
  중앙 영역은 나머지 공간을 사용한다. 로그 표 헤더는 y=276 부근, 데이터 행은 약 44px 높이다.
  왼쪽 보드 목록 헤더는 68px, 각 보드 행은 96px이다.
- 제목 28px 전후, 보드 이름 26px, 주요 UI 16px, 도움말 14px, 로그는 16px monospace를 기준으로 맞춘다.
- 어두운 남색 바탕, 밝은 글자, 얇은 회색 구분선. 선택 노드의 왼쪽 라임색 선과 상태 조회 버튼만 강하게 강조한다.
- refresh/search/pause/save/trash/chevron/check 아이콘은 동일한 가는 선 스타일로 구성한다.
- 반복 컴포넌트: node row, 상태 dot+문자, outline button, primary button, segmented filter,
  key/value row, log row. 중첩 카드나 새로운 통계 카드·차트는 추가하지 않는다.
- 화면 문구는 시안의 헤더·보드·메시지 로그·Mesh 상태·On/Off 시험·송신 출력을 기준으로 사용한다.
  실제 연결 전/실패/오래됨 상태에 필요한 문구와 안전 경고는 기능상 필요한 변형으로 둔다.

시안의 다음 부분은 구현 시 의도적으로 바로잡는다:

1. 예시 `pub=0xC001` 행의 UART RX 분류는 STATUS로, `id=0x13 result=queued` 행은 UART RX로 분류한다.
2. header와 footer의 예시 표시를 실제 앱에서 고정 표시하지 않는다. 실제 앱은 로컬 연결 상태·마지막 갱신 시간으로 표시한다.
3. 시안의 D6 연결·주소·키·카운터는 초기 데이터로 사용하지 않는다. 연결 전에는 빈 값과 미연결 상태를 보여 준다.
4. 원문과 버튼의 읽기 쉬움을 위해 생성 이미지의 작은 글자 왜곡은 코드에서 정확한 문구로 교정한다.
5. 이미지의 미세한 표면 잡음은 bitmap 배경으로 복제하지 않고 지정된 단색 CSS로 구현한다.
6. 상태 갱신 간격·마지막 수신 시각, 명령 전달 알림, 가상 USB 시험 표시를 추가한다.
   표에서는 ESP 로그 접두어를 접어 메시지를 읽기 쉽게 하고, 행 선택/저장에서는 전체 원문을 유지한다.
7. 실제 상태의 이벤트 송신·구독 준비를 별도 행과 문자로 표시한다. 오류만 필터는 boolean checkbox로 구현한다.
   좁은 화면은 보드/메시지 로그/제어 전환으로 같은 기능을 제공한다.

생성 지시: 전체 화면의 한국어 USB Mesh 개발 도구, 왼쪽 세 노드·중앙 로그·오른쪽 제어,
검은 남색과 라임색, 표시된 예시 데이터, 실제 펌웨어 명령만 사용, 마케팅 영역·가짜 그래프 제외.

## 8. 검증과 완료 조건

1. fake serial을 사용한 단위 테스트: serial identity, 무리셋 open, 허용 명령, 줄 파싱, 상태 만료,
   USB 분리, 동시 연결·쓰기, 종료·해제, queue 상한.
2. API 테스트: 잘못된 Host/Origin/board/command 거절, 연결 전 명령 거절, stream 종료 정리.
3. UI 테스트: 연결/해제 상태, disabled 제어, 로그 필터·검색·일시정지·저장, 좁은 화면·키보드.
4. production build와 타입 검사. UI 테스트는 테스트 데이터임을 명시하며 하드웨어 PASS로 취급하지 않는다.
5. 승인된 시안과 브라우저 screenshot을 `view_image`로 비교: layout, typography, palette,
   spacing, controls, 화면 문구를 확인하고 차이를 수정/기록한다.
6. 실장치 검증은 기존 모니터가 포트를 사용하지 않을 때 한 대를 명시적으로 연결하여 진행한다.
   상태 조회와 로그 수신부터 확인하고, On/Off·송신 출력 변경은 별도 명시적 사용자 조작으로 검증한다.

실장치 검증을 하지 못했다면 앱 빌드/모의 검증 결과와 구분해서 인계한다.
이 설계 단계에서는 USB 포트를 열거나 명령을 전송하지 않았다.

## 9. 참고

- 로컬 기준: `layers/layer-8/main/main.c`, `main/serial_command.c`, `main/mesh_node.c`,
  `main/bridge_runtime.c`, `tools/check_uart_diag.py`, `tools/fast_check.py`.
- [Web Serial 공식 설명](https://developer.chrome.com/docs/capabilities/serial): 대안 비교용.
- [FastAPI WebSocket 공식 문서](https://fastapi.tiangolo.com/advanced/websockets/): 로컬 실시간 스트림 구현 참고.

저장소 루트는 Git 저장소가 아니므로 이 설계를 위해 새 Git 저장소를 만들거나 다른 하위 프로젝트에 commit하지 않는다.
