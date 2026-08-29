# 2026-08-28 저장소 재구성 결과

[구조 결정](../../decisions/0001-repository-layout.md) · [현재 프로젝트](../../../README.md)

`code/`를 제품별 최상위 경로로 옮기고, 원본 사본의 앱·도구·추가 문서를 분류했습니다. Layer 0–7과 이전 ESP32 소스는 Git 이력에 보존합니다. 원본 STM32의 기능 차이는 별도 패치로 보존하고 제품에는 자동 적용하지 않았습니다.

## 파일 보존

- [파일별 원본·목적지·SHA-256·처리 방식](manifest.json): 작업 직전 존재한 1,012개 파일 모두를 대응했습니다. 이전 턴에서 삭제한 학습용 `code/layers/`는 삭제 전 Git 이력에 있습니다.
- 현재 펌웨어의 C/H·IOC·Swift·음원과 과거 이관 JSON 등 218개 보존 대상의 바이트 일치를 확인했습니다.
- 원본 사본의 추적 파일 520개는 저장소 밖 백업과 SHA-256이 일치합니다. 원본의 로컬 의존성도 함께 보존했습니다.
- 현재 소스에서 제외한 파일은 명시된 Git 커밋에 원본 바이트가 있음을 확인했습니다.
- STM32 추가 변경 15개 파일은 `experiments/stm32-output-test/changes.patch`에 보존했습니다. 임시 복사본에 실제 적용한 결과가 프로젝트명·경로를 정규화한 원본 변경본과 일치하며, 그 복사본의 호스트 검사를 실행했습니다.

실행 Mac의 저장소 옆 `nostos-backup-20260828-223342/`에 전체 소스 스냅샷, 이전 미커밋 diff, 원본 사본, 검사 로그를 보존했습니다. Git 기록을 다시 쓰거나 원격으로 push하지 않았습니다.

## 새 경로에서 실행한 검사

| 대상 | 결과 |
| --- | --- |
| Protocol·STM32·ESP32·Communication·GPS C host | Debug/Release/sanitized 15개 구성, 총 54회 테스트 PASS |
| Python fast-check·메시지 통합·저장소 검사 단위 테스트 | 14 + 6 + 5개 PASS |
| Mesh Console | backend 20개, UI 7개 PASS, TypeScript/Vite build PASS |
| Swift GPSCore | 7개 PASS |
| iPhone 앱 | Xcode 26.4, generic iOS, CODE_SIGNING_ALLOWED=NO build PASS |
| STM32 추가 변경 패치 | 임시 복사본에 적용 후 Debug/Release/sanitized 각 5개, 총 15회 PASS |
| 모니터 스크립트 | 가짜 IDF로 6개 호출 검사 PASS, 실제 serial 미접속 |
| 메시지·코덱 도구 | 새 경로의 help와 가짜 장치 조건의 결과 폴더 생성 검사 PASS |
| 로컬 문서 링크 | 오류 0건. 최종 숫자는 `python3 tools/check_repository.py`로 확인 |
| 신규 변경 공백 검사 | 새로 추가·수정한 텍스트 줄의 공백 오류 0건 |
| STM32·ESP32 대상 펌웨어 전체 빌드 | 미실행: Arm GNU Toolchain·ESP-IDF가 이 Mac에 없음 |

[검사 요약과 로그 해시](verification.json)를 함께 보존합니다. 전체 호스트 검사는 `bash tools/test-host.sh`, 앱 검사는 각 앱의 README로 재실행합니다.

최초 통합 호스트 스크립트는 macOS Bash 3.2의 `set -u`와 빈 배열 조합에서 중단했습니다. 해당 호환성 문제를 수정한 뒤 전체 검사를 다시 실행해 통과했습니다. 최초 실패 로그도 외부 백업에 남겼습니다. Mesh Console 검사에는 기존 Starlette/httpx 사용 중단 예정 경고 1건이 있으며 검사 실패는 아닙니다. 의존성 버전은 변경하지 않았습니다.

## 증거와 장치 경계

원본에 없던 결과 파일·외부 SDK 문서의 링크는 미포함 사실을 표시했고 [manifest](manifest.json)에 기록했습니다. 성공 증거 파일을 새로 만들어 대체하지 않았습니다. 기존 로그·manifest의 당시 경로·해시는 그대로 보존했습니다.

사용자 승인 후 Mac 웹 서버 8787·8788을 정상 종료했고 자동 재시작하지 않았습니다. Flash, reset, 실제 serial 접속, Mesh 키·provisioning 변경, Git commit/push는 수행하지 않았습니다. 빌드·모의 시험은 실물 송수신이나 실제 출력 성공을 뜻하지 않습니다.
