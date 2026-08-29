# Nostos 이관 기록

날짜: 2026-08-28. [팀원 시작](../../getting-started/README.md) · [전체 원문 목록](IMPORTED-DOCUMENTS.md)

## 이관 범위

기존 `esp-ble`의 코드·문서·이미지·관찰 로그를 새 `nostos`의 `code/`와 `docs/`로 분리했다. 원본 폴더에서는 파일 이동·수정·삭제나 Git commit/reset/remote 변경을 하지 않는다.

STM32 이름은 `nostos_stm32`, 팀 ESP32 이름은 `nostos_esp32`다. **사용자의 후속 지시에 따라 팀 ESP32의 기준은 Layer 8이다.** 예전 통합본은 `code/legacy/esp32-event-bridge/`에 보존하고 배포 기준에서 제외했다.

## 코드 변경 내역

| 변경 | 이유와 한계 |
| --- | --- |
| STM32 CMake·ioc 이름 | `bike_swarm_guard` → `nostos_stm32`. 핀·주변장치·센서 기본값은 유지 |
| 팀 ESP32 | Layer 8 main·설정·UART 진단·도구·호스트 테스트 복사. 프로젝트명만 `nostos_esp32` |
| ESP32 공통 codec 참조 | Layer 8 복사본 대신 `code/common/protocol/`. C 구현·메시지 ID는 동일 |
| 공통 protocol 테스트 경로 | 새 팀 ESP32의 `serial_command.c`를 참조 |
| 이전 통합본 함수명 오타 | `mesh_node_send_onofSf` → `mesh_node_send_onoff`. legacy 보존본 빌드 차단 수정 |
| GPS 호스트 Release 검사 | `-UNDEBUG` 추가. Release에서도 assert 기반 테스트 검사를 실행. 펌웨어 로직 변경 없음 |
| 실행 스크립트 기본 경로 | 프로젝트 위치에서 경로 계산, 사용자 SDK 경로를 `$HOME`/환경변수로 지정 |

`MyApp` 실행 코드·센서 판정·버튼·음원은 바꾸지 않았다. 새 RTOS 구조, 데이터 wire 확장, 센서별 preset, SharedState 연결은 추가하지 않았다.

## 문서와 증거 보존

기존 문서에는 원본 위치와 현재 시작 안내를 표시했다. 상대 링크를 새 위치로 갱신했으며 사용자 이해·질문·과거 성공/미확인 기록은 보존했다. 기존 `verified/`가 참조하는 빌드 폴더 내부의 실물 시험 증거 8개도 `docs/verified/evidence/`에 따로 보존했다.

기존 문서 속 당시 명칭·절대 경로·명령은 역사적 기록이다. 현재 빌드 명령은 [팀원 시작](../../getting-started/README.md)을 따른다.

## 검사 결과

이 표는 실제 실행 결과를 갱신한다. 빌드 PASS는 Flash·실물 수신 성공을 뜻하지 않는다.

| 대상 | 결과 |
| --- | --- |
| STM32 `nostos_stm32` | Debug·Release 빌드 PASS |
| 팀 ESP32 `nostos_esp32` | Layer 8 기반 새 경로 빌드 PASS |
| 이전 통합 ESP32 | 기준 변경 전에 비교용 빌드 PASS. 배포 기준 아님 |
| 공통 protocol | Debug·Release·ASan/UBSan, 각 3개 PASS |
| STM32 host | Debug·Release·ASan/UBSan, 각 3개 PASS |
| Communication Module | Debug·Release·ASan/UBSan, 각 7개 PASS |
| 학습 Layer 8 host | Debug·Release·ASan/UBSan, 각 4개 PASS |
| 팀 ESP32 host | Debug·Release·ASan/UBSan, 각 4개 PASS |
| GPS codec host | Debug·Release·ASan/UBSan PASS. 최초 Release 실패 후 테스트 옵션 수정·재검사 |
| fast-check Python | 14개 PASS. 가짜 로그 검사이며 serial port 미접속 |
| GPSCore Swift | 7개 PASS |
| 저장소 검사 도구 | 도구 단위 테스트 4개 PASS |
| 학습 Layer·참고 ESP32 예제 전체 빌드 | Layer 0–8 9개, ESP32-S3 GPS·ESP32-C3 예제 2개, 총 11개 PASS |
| iPhone 앱 | Xcode 26.6, generic iOS destination, CODE_SIGNING_ALLOWED=NO 빌드 PASS. 설치·실기 실행 아님 |
| 최종 문서 링크·이관 manifest·Git 검증 | 로컬 경로 검사 PASS, 원본 파일·Git 상태 변경 없음, 생성 산출물 추적 없음 |

## Git·배포 상태

기존 STM32 HEAD `f7335998256e637cd56d443f4205aee5c97946ba`의 전체 이력을 새 이관 커밋의 부모로 보존한다. 기존 작업 트리의 미커밋 파일도 파일 단위로 이관한다. 저장소에는 중첩 `.git`이나 submodule을 만들지 않는다.

로컬 이관과 빌드·검사를 완료했다. 이 문서를 포함한 최종 이관 커밋을 지정된 `nostos/main`에 일반 push하며, 원격 HEAD와 로컬 HEAD의 일치 여부로 배포를 확인한다. 새 저장소에는 하나의 Git root만 둔다.

## 이번 작업에서 하지 않은 것

보드 Flash/reset, 실제 serial port 접속, Mesh 키·provisioning·그룹 설정 변경, 새 펌웨어의 실물 송수신 시험은 하지 않았다. 실제 NetKey/AppKey·개인 자격증명은 배포하지 않는다.

## 보존·검사 자료

- [파일별 원본/목적지/SHA-256/수정 사유](migration/source-manifest.json)
- [빌드 폴더에서 회수한 실물 증거 대응](migration/evidence-link-mapping.json)
- [검사 실행 이력](migration/host-execution-history.json)과 [로그 색인](migration/verification-log-index.json)
- [기존 공백 경고 분류](migration/whitespace-summary.json): 원본 HAL·문서 등의 1,922건을 그대로 보존했고, 새로 만든 경고는 0건이다. 전체 새 파일 diff에 대한 git diff --check가 깨끗하다는 뜻은 아니다.
- [최종 원본·공백 대조](migration/final-source-audit.json): 직접 작성·수정한 코드와 문서의 새 공백 오류는 0건이다. 도구의 실행 로그 원문에 있는 공백 2건은 로그 해시와 함께 보존했다.
- [추가 ESP32 빌드 결과](migration/extra-build-results.json): Layer 0–8 및 두 참고 예제의 실제 종료 코드와 로그.
- [참고 이미지 출처 대조](migration/public-image-provenance.json): BLE Mesh 이미지 10개는 Espressif 공개 문서 이미지와 SHA-256이 일치한다. 이미지 속 AppKey 표시는 공개 교재 예시이며 팀의 실제 키 export가 아니다.

최초 GPS Release 실패 로그는 삭제하지 않았다. `-UNDEBUG` 수정 뒤의 성공 로그와 함께 남겼다. Communication Module의 최초 sanitizer 이름 실행은 잘못된 CMake 옵션을 사용했으므로 sanitizer 증거로 쓰지 않는다. `COMM_ENABLE_SANITIZERS=ON`으로 재구성·재빌드·재검사했고 실제 컴파일 옵션을 확인한 `comm-real-sanitizers.log`가 최종 근거다.
