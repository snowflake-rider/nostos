# 테스트 — 여기서 시작

**코드·핀·두 MCU 빌드·앱 검사 한 번에:**

```sh
bash tests/run.sh
```

보드 연결 없이 실행합니다. 설치·Flash·설정 변경·송신은 하지 않습니다.
실패한 항목이 있어도 나머지 독립 검사는 계속해서 전체 상황을 보여 줍니다.

| 단계 | 확인하는 것 | 개별 실행 |
| --- | --- | --- |
| [01 핀](system/01-pins/README.md) | STM32 IOC/초기화, ESP32 핀·UART 계약 | `bash tests/run.sh pins` |
| [02 코드](system/02-host/README.md) | 기존 회귀 + 메시지·출력 모의 검사 | `bash tests/run.sh logic` |
| [03 STM32](system/03-stm32/README.md) | Debug/Release 전체 컴파일·링크 | `bash tests/run.sh stm32` |
| [04 ESP32](system/04-esp32/README.md) | ESP32-S3 전체 컴파일·링크 | `bash tests/run.sh esp32` |
| [05 앱](system/05-apps/README.md) | Mesh Console/TUI 테스트·빌드 | `bash tests/run.sh apps` |
| [06 USB](system/06-usb/README.md) | D6·76·B6 상태 조회, 기본 실행에서는 제외 | `bash tests/run.sh usb` |
| [Mesh 01–04](mesh/README.md) | 준비 → 6회 송수신 → 반복 → Relay 비교 | 송신은 `--send` 필수 |

각 단계 폴더의 `run.sh`로도 실행할 수 있습니다. 모든 명령은 저장소 루트 기준입니다.
`bash tests/run.sh list`는 목록, `--json`은 자동화용 최종 JSON만 출력합니다.
메시지/출력 검사는 코드 회귀에 이미 포함되어 중복 실행하지 않습니다. 그것만 보려면 `bash tests/run.sh protocol`.

## 결과 읽는 법

- `PASS`: 표시된 **코드/빌드 검사** 통과. 실물 성공 아님.
- `FAIL`: 실제 검사/컴파일 실패. 같은 이름의 `.log`에서 오류를 확인합니다.
- `BLOCKED`: 도구·장치·해석 조건이 부족해 판정 불가. 안내된 준비부터 확인합니다.
- `READ`: USB 상태 조회 성공. 전체 Mesh 설정이나 무선 성공 아님.
- `NOT_TESTED` / `NOT_RUN`: 확인하지 않음. `CANCELLED`: 사용자 중단.
- Mesh의 `READY`는 사전 준비 보고, `OBSERVED`는 C000 로그 일치 관찰입니다. ACK/정확한 패킷 손실률/다중 홉 경로 증명이 아닙니다.

통합 검사 종료 코드: **0=요청한 검사/조회 완료, 1=실패, 2=준비 차단, 130=중단**.
여러 항목에서 FAIL과 BLOCKED가 함께 나오면 1입니다. 미검증 실물 항목은 JSON에도 남습니다.
`--timeout 1200`은 명령별 제한 초입니다. Ctrl+C는 이번 실행이 만든 자식 프로세스만 종료합니다.

상세 로그·기계용 `summary.json`은 **매번 새 `build/test-results/<실행>/` 폴더**에 남습니다.
USB 상세는 같은 폴더의 `usb.json`, Mesh 증거는 기존 `build/hardware-results/`에 남습니다.
기존 결과는 덮어쓰지 않습니다. 결과 폴더는 전체 빌드 산출물을 포함하므로 용량이 클 수 있습니다.

## 준비 / 아직 직접 확인할 것

Python 3, CMake/C 컴파일러, STM32 Arm GNU+Ninja, ESP-IDF **5.5.5**, 앱의 기존 개발 환경이 필요합니다.
도구는 PATH 또는 설치된 `$HOME/.local/share/nostos-toolchains`에서 찾습니다.
다른 위치는 `NOSTOS_TOOLCHAINS`, `ESP_IDF_PATH`, `IDF_TOOLS_PATH`로 지정합니다. 셸 프로필은 변경하지 않습니다.
소스 검사 기준은 현재 `firmware/stm32`, `firmware/esp32`이며 실험용 모든 MCU 프로젝트의 빌드는 포함하지 않습니다.

**실제 배선/전압, 센서 측정, STM32↔ESP32 양방향 UART, 상대 STM32 LED·부저·음성,
C001 이벤트 전달, 통제된 다중 홉, 장시간 RF 안정성은 자동 PASS로 처리하지 않습니다.**
[배선 안내](../docs/hardware/wiring.md)와 [실물 검증 순서](../docs/getting-started/README.md)를 따라 별도 확인합니다.

기존 [개발자 모의 검사](integration/README.md), `tools/test-host.sh`, 고급 Mesh 명령은 그대로 사용할 수 있습니다.

## v2 메시지 전체 검사

[message-protocol](message-protocol/README.md): `bash tests/message-protocol/run.sh`로 15종 메시지·mock Relay·실제 C 송수신/출력을 한 번에 검사합니다. `--all`은 기존 호스트 회귀도 포함합니다. 실제 전파/장비는 자동 조작하지 않습니다.
