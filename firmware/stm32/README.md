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

`NOSTOS_PROTOCOL_V2=ON` 배포 빌드는 현재 D6에 연결된 STM32를 `source2`, deployment session을
`1`로 사용합니다. 마지막 Flash sector 6·7은 CRC와 마지막 commit word가 있는 교대 journal로
예약해 송신 순번, 승인 window, FALL/SOS 기록을 복구합니다. linker는 앱을 앞 256KB에 제한해
journal과 겹치면 빌드 단계에서 실패합니다. ST-LINK VCP의 첫 줄이
`NOSTOS_V2_BOOT=READY source=2 session=1`인지 확인한 뒤에만 UART/Mesh 시험을 진행합니다.

실보드 설치에는 `scripts/v2-flash.sh`만 사용합니다. 정확한 ST-LINK serial과 512KB 용량을
검증한 뒤 하나의 halted GDB session에서 전체 Flash 백업, ELF 앱 기록, 앱 readback, sector 6·7
journal의 전후 byte 일치를 차례로 확인합니다. 비교가 끝난 후에만 reset하므로 정상 부팅의 첫
checkpoint 기록과 programmer 보존 검사를 혼동하지 않습니다. macOS USB capture 때문에 관리자
권한이 필요하지만 스크립트는 mass erase를 실행하지 않습니다. 생성된 백업은 무시되는
`build/hardware-results/` 아래에 owner-only 권한으로 보관합니다.
