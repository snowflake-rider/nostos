# Nostos

STM32가 버튼·센서 입력과 LED·부저·음성 출력을 처리하고, ESP32-S3가 Bluetooth Mesh로 라이더 사이의 이벤트를 전달합니다.

**현재 ESP32 구현은 Layer 8 기반 `firmware/esp32/` 하나입니다. 이후 기능도 이곳에서 확장합니다.**

## 시작하기

- [ESP32 설정 조회 TUI — 사람과 Codex 공용](apps/esp32-tui/README.md)
- [핀·코드·STM32/ESP32 빌드·앱 검사 — 한 명령](tests/README.md): `bash tests/run.sh`
- [ESP32 세 대 시험 — 01부터 순서대로](tests/mesh/README.md)
- [빌드·배선·시험 순서](docs/getting-started/README.md)
- [전체 문서](docs/README.md)
- [구조와 의존성](docs/architecture/README.md)
- [저장소 구조 결정](docs/decisions/0001-repository-layout.md)

## 프로젝트 위치

| 위치 | 역할 |
| --- | --- |
| [firmware/stm32](firmware/stm32/README.md) | STM32 NUCLEO-F411RE 펌웨어, `nostos_stm32` |
| [firmware/esp32](firmware/esp32/README.md) | Layer 8 기반 UART ↔ Mesh 펌웨어, `nostos_esp32` |
| [apps/mesh-console](apps/mesh-console/README.md) | Mac USB 콘솔 웹 앱 |
| [apps/esp32-tui](apps/esp32-tui/README.md) | ESP32 설정 요약 TUI, 공용 JSON 조회 |
| [apps/ios-gps-mesh](apps/ios-gps-mesh/README.md) | iPhone GPS Mesh 앱 |
| [libs/protocol](libs/protocol/README.md) | STM32·ESP32 공통 메시지 ID, codec, queue |
| [tools](tools/README.md) | 호스트 검사, 환경 확인, 모니터, 실물 관찰 도구 |
| [tests/integration](tests/integration/README.md) | 구성 요소 간 메시지 경로의 모의 로그 검사 |
| [experiments](experiments/README.md) | 별도 통신 API, 참고 예제, 미통합 STM32 변경 패치 |

각 프로젝트는 자체 빌드 설정과 단위 테스트를 가집니다. 저장소 전체를 하나의 SDK나 빌드 시스템으로 합치지 않습니다. SDK·의존성 캐시·빌드 산출물은 Git에 넣지 않습니다.

## 보드 없이 검사

CMake, C 컴파일러, Python 3, Make 또는 Ninja가 필요합니다.

```sh
bash tools/test-host.sh
```

이 명령은 C 호스트 검사(Debug/Release/ASan·UBSan), Python 모의 로그 검사, 저장소 링크 검사를 실행합니다. 앱 검사는 각 앱 README를 따릅니다. **호스트 검사에는 USB 포트 접속·Flash·reset이 없습니다.**

## 보존 범위와 주의

- 학습용 Layer 0–7과 이전 ESP32 구현은 현재 소스 트리에서 제외했습니다. [과거 설명·검증 기록](docs/archive/README.md)과 Git 이력에 남아 있습니다.
- `esp-ble-unorganized/`의 미이관 앱·도구·문서는 새 위치로 옮겼습니다. 원본의 STM32 버튼 출력 시험·오디오 변경은 [재적용 가능한 패치](experiments/stm32-output-test/README.md)로 보존했으며 현재 펌웨어에 자동 반영하지 않았습니다.
- 핀, 메시지 ID, 센서 기본값, 펌웨어 동작은 폴더 이관을 이유로 바꾸지 않았습니다.
- [과거 실물 시험](docs/verification/README.md)은 당시 소스·배선·보드의 기록입니다. 현재 경로에서의 빌드 성공이나 호스트 테스트는 새 실물 검증을 뜻하지 않습니다.
