> 이관 원문: `stm32-project/docs/superpowers/plans/2026-08-28-dual-firmware-repository-layout-implementation.md`. 현재 실행 경로는 [팀원 시작 안내](../../../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 이중 펌웨어 저장소 경계 정리 구현 계획

사용자가 [저장소 구조 설계](../specs/2026-08-28-dual-firmware-repository-layout-design.md)를 승인했다. STM32와 ESP32-S3의 형제 project 구조를 유지하고 공유 event ID header의 소유권만 바로잡는다.

1. `integration/stm32/MyApp/common/message_type.h`를 내용 변경 없이 `common/protocol/message_type.h`로 이동한다.
2. STM32 CMake에 `common/protocol` include path를 추가한다.
3. ESP32 component와 host protocol CMake에서 `integration/stm32/MyApp/common` 의존을 제거한다.
4. 이전 `message_type.h` 위치를 가리키는 설계·배선·사용 문서를 새 위치로 갱신한다.
5. 이전 의존 경로가 남지 않았는지 검색하고 Host Debug/Release/sanitizer, STM32 Debug/Release feature matrix, ESP32-S3 build를 실행한다.
6. event ID 값과 wire format이 바뀌지 않았고 변경 범위에 build output이나 다른 사용자 작업이 섞이지 않았는지 확인한다.

보호 대상: STM32 `.ioc`, CubeMX 생성 코드, sensor/output logic, UART/Mesh payload, queue 동작, 기존 사용자 미커밋 변경. Flash, NVS 변경, board runtime 주장, 원격 push는 하지 않는다.

완료 기준: 세 build 계층이 통과하고 `common/protocol`과 ESP32가 STM32 `MyApp`을 include하지 않으며, 문서의 유일한 event ID 원본이 `common/protocol/message_type.h`로 일치한다.
