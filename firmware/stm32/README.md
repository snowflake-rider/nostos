# Nostos STM32

NUCLEO-F411RE의 버튼·센서·LED·부저·VS1003B·UART 처리. 빌드 이름은 `nostos_stm32`입니다.

## 빌드

Arm GNU Toolchain, CMake, Ninja를 PATH에 준비합니다.

```sh
cd firmware/stm32
cmake --preset Debug
cmake --build --preset Debug
```

Release는 같은 위치에서 `cmake --preset Release`, `cmake --build --preset Release`로 빌드합니다. CubeMX 입력은 [nostos_stm32.ioc](nostos_stm32.ioc)입니다. `Core/`, `Drivers/`, `MyApp/`의 기존 경계를 유지합니다.

## 테스트와 문서

- 저장소 루트에서 `bash tools/test-stm32-host.sh`: 보드 없는 host-tests.
- [팀 시작·배선](../../docs/getting-started/README.md)
- [SharedState](../../docs/architecture/shared-state.md)
- [버튼·UART 기록](../../docs/verification/stm32-button-uart.md)
- [추가 출력 시험 패치](../../experiments/stm32-output-test/README.md): 현재 펌웨어에 미적용.

이번 폴더 정리에서는 핀, 센서 기본값, 메시지 ID, 런타임 C 코드를 바꾸지 않았습니다.

## v2 프로토콜 선택 경로

공통 v2 codec·UART ISR ring·상태 적용·RGB/부저/오디오 경로를 추가했습니다. CMake `-DNOSTOS_PROTOCOL_V2=ON`으로 선택하며 기본은 기존v1입니다. 신뢰된 세션/사건 복구를 담당하는 boot provider가 없으면 v2는 NOT_READY로 시작합니다. [구현 계약과 배포 경계](../../libs/protocol/V2.md) · [호스트 one-stop 테스트](../../tests/message-protocol/README.md).

v2 선택 시 공개 `message_protocol_service_init`을 ELF에 보존해 codec·오디오 경로까지 실제 링크합니다. 이것이 기본 boot hook의 NOT_READY를 해제하지는 않습니다. 저장소 루트의 `bash tests/message-protocol/run.sh --targets`는 설치된 도구로 v2 Debug/Release 및 ESP32-S3를 임시 복사본에서 빌드하고, 링크된 경로도 검사합니다. Flash하거나 보드 설정을 바꾸지 않습니다.
