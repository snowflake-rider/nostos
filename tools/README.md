# 개발·검사 도구

**핀·코드·STM32/ESP32 빌드·앱 검사 한 번에:** `bash tests/run.sh` — [짧은 단계별 안내](../tests/README.md).
화면에는 요약만, 상세 로그/JSON은 실행별 새 폴더에 저장합니다. USB 조회는 `bash tests/run.sh usb`로 별도 실행합니다.

**세 보드 설정을 빠르게 읽기:** `bash scripts/esp32-scan` — [사용법·조회 한계](../scripts/README.md).

**ESP32 세 대를 시험하려면 [단계별 시험 폴더](../tests/mesh/README.md)부터 보세요.**
아래는 개발자용 개별 도구와 고급 실행 방법입니다. 기존 명령은 계속 사용할 수 있습니다.

| 명령 (저장소 루트) | 동작 |
| --- | --- |
| `bash tools/test-host.sh` | C Debug/Release/sanitizer, Python, 저장소 링크 검사. 하드웨어 접근 없음 |
| `bash tools/test-stm32-host.sh` | STM32 host-tests만 sanitizer로 검사 |
| `python3 tools/check_repository.py` | 로컬 문서 링크·소스 배치 검사 |
| `bash tools/check-esp-idf.sh` | ESP-IDF 환경 확인 |
| `bash tools/device_profile.sh` | Mac USB 장치 정보 확인 |
| `ESP_PORT=<port> bash tools/monitor.sh` | 현재 ESP32 프로젝트의 USB 콘솔 모니터 |
| `bash tools/hardware/run_mesh_repeat.sh check` | 기존 Mesh Console을 통한 읽기 전용 C000 반복 시험 사전 점검 |

[반복 송수신·Relay OFF/ON/OFF 검사](../docs/verification/mesh-repeat.md)는 명시적 `run --send`에서만
ON/OFF 시험을 송신합니다. C000 설정이 필요하며 C001 이벤트/STM32 시험과 구분합니다.

`test-host.sh`는 Ninja가 없으면 Make를 사용합니다. `NOSTOS_TEST_BUILD_DIR`, `CMAKE_GENERATOR`, `NOSTOS_PYTHON`으로 검사 환경을 지정할 수 있습니다.

[hardware/](hardware/)의 메시지·코덱 도구는 실물 USB를 열 수 있습니다. 실행 전 [검증 안내](../docs/verification/index.md)를 읽고 다른 모니터를 종료합니다. 새 관찰 결과는 Git에서 제외하는 `build/hardware-results/`에 저장합니다. 보존할 증거만 검토 후 `docs/verification/`로 옮깁니다.
