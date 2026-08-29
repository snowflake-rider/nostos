# ESP32 상태 빠르게 읽기

**전체 코드·핀·MCU 빌드 검사:** `bash tests/run.sh` — [단계별 검사 안내](../tests/README.md).
USB 조회 결과도 파일로 남기려면 `bash tests/run.sh usb`를 사용합니다.

**화면으로 보려면:** `bash scripts/esp32-tui` — `1/2/3` 보드 선택, `r` 조회, `h` 안내, `q` 종료.
**JSON으로 보려면:** `bash scripts/esp32-tui --json`. 두 명령 모두 내부 서버를 자동 준비합니다.
자동 서버는 활성 조회·웹 연결이 없으면 약 10초 후 종료하며, 사용자가 켠 서버는 건드리지 않습니다.
보드 없이 보기: `bash scripts/esp32-tui --demo`. [TUI 설치·사용법](../apps/esp32-tui/README.md)

저장소 루트에서:

```sh
bash scripts/esp32-scan
```

D6·76·B6를 USB serial + VID/PID로 식별하고, **기존 [Mesh Console](../apps/mesh-console/README.md)**을
통해 새 STATUS를 기다립니다(기본 약 1–10초). Console 서버는 실행되어 있어야 합니다.
웹 화면은 닫혀 있어도 됩니다. 미연결 보드는 포트 점유 확인 후 상태 조회용으로 연결합니다.
사용 중인 다른 모니터는 강제 종료하지 않으며, 별도로 USB 포트를 중복해서 열지 않습니다.

| 명령 | 용도 |
| --- | --- |
| `bash scripts/esp32-scan --json` | 세 보드 상세 JSON |
| `bash scripts/esp32-scan --ensure-console` | 필요한 서버를 자동 준비하고 조회(TUI와 같은 방식) |
| `bash scripts/esp32-scan --usb-only` | USB 감지만, Console/시리얼 연결 없음 |
| `bash scripts/esp32-scan --no-connect` | 이미 Console에 연결된 보드만 조회 |
| `bash scripts/esp32-scan --out build/hardware-results/scan-01.json` | 새 JSON 파일 저장, 기존 파일 덮어쓰기 금지 |

**알려주는 값:** 주소, C001 Publication/구독/키 인덱스/TTL/Period/Publication 재전송,
이벤트 준비, OnOff Client 준비·상태, 캐시된 Relay 보고.

**못 읽는 값:** 모델별 실제 AppKey Bind 목록, C000 Publication 세부 설정, OnOff Server 구독,
현재 Relay·Relay 재전송. 펌웨어가 USB에 노출하지 않아 `unavailable_fields`로 표시합니다.
키 원문은 읽거나 저장하지 않습니다. `net/app`은 **C001 모델의 키 인덱스**이며 키 자체가 아닙니다.

새 STATUS를 받아도 내부 Mesh 설정 스냅샷은 오래됐을 수 있습니다. 앱 화면과 값이 다르면
설정 실패/성공을 단정하지 않습니다. `STATUS_READ_PARTIAL_CONFIG`와 종료 코드 0은
**세 보드의 요약 조회 성공**이지 전체 설정·무선 시험 통과가 아닙니다. 부분 조회/오류는 2입니다.

설정·ON/OFF·출력 변경·reset·Flash는 하지 않습니다. 종료 시 기존 UI 연결은 끊지 않습니다.
웹 화면도 없으면 Console의 기존 정책에 따라 약 2초 후 USB가 해제됩니다.
Console의 `.venv`를 사용하며 자동 설치하지 않습니다(`ESP32_SCAN_PYTHON`으로 재정의 가능).

[단계별 송수신 시험](../tests/mesh/README.md)은 별도입니다. 전체 설정의 실시간 조회는 향후
펌웨어 진단 명령 확장과 별도 승인된 Flash 또는 nRF Mesh의 장치 재조회가 필요합니다.
