# Nostos ESP32 — Layer 8 기반 배포 펌웨어

[팀원 시작](../../00-team/START.md) · [실제 코드](../../../code/firmware/esp32/) · [기존 Layer 8 설명](../../layers/layer-8/README.md)

사용자가 Layer 8을 더 최신 작업본으로 지정하여 팀 배포 기준으로 선택했습니다. UART 진단, USB Serial/JTAG 콘솔, UART1 ↔ Mesh bridge, 호스트 검사와 관찰 도구를 함께 가져왔습니다.

## Layer 8과 달라지는 것

- 빌드 프로젝트명과 부팅 로그의 프로젝트명: `nostos_esp32`.
- `main/CMakeLists.txt`와 `host-tests/CMakeLists.txt`: 복사된 `common/` 대신 `code/common/protocol/`을 참조.
- 파일 위치: `code/firmware/esp32/`.

그 외 Layer 8 C 코드와 UART·Mesh 설정은 유지합니다. 공통 protocol의 C 구현은 Layer 8 사본과 동일하며, 메시지 ID 헤더의 기존 차이는 설명 주석뿐입니다.

기존 통합 ESP32 코드는 [legacy](../../../code/legacy/esp32-event-bridge/)에 보존했습니다. 그 코드의 `mesh_node_send_onofSf` 오타는 이전 통합본 빌드를 막아 올바른 함수명으로 수정했지만 팀 배포 기준으로 사용하지 않습니다.

새 경로의 빌드·검사 결과는 [이관 기록](../../04-records/NOSTOS-MIGRATION.md)을 확인합니다. 과거 Layer 8 실물 기록을 새 빌드의 Flash·무선 수신 완료로 표시하지 않습니다.
