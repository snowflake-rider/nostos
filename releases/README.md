# NOSTOS 릴리스 기록

이 디렉터리는 Git에 남길 릴리스 metadata와 검증 요약을 보관합니다. 저장소 구조와 전체 lifecycle은
[`STRUCTURE.md`](../STRUCTURE.md)를 기준으로 합니다.

## 분류

- `index.json`: 공식 릴리스와 과거 baseline 기록의 색인
- `baselines/`: 구조 변경 전 빌드·배포 상태를 보존한 비공식 기록
- 향후 공식 manifest: tag, source commit, 번들 SemVer, UART/Mesh protocol 버전, 타깃별 파일명·크기·
  SHA-256, 검증 범위와 미검증 항목을 기록

baseline은 공식 릴리스나 재배포 가능한 패키지가 아닙니다. 당시 경로·hash·검증 범위를 그대로
보존하며, 현재 경로에 맞추기 위해 수정하지 않습니다.

## 바이너리와 로컬 정보

패키지 바이너리는 `firmware/out/releases/nostos-vN.M.P/` 또는 별도 artifact 저장소에 보존하고 Git에는
추가하지 않습니다. 실제 장비 serial/port, Mesh key, NVS backup, 개인 위치 원본도 이 디렉터리에
기록하지 않습니다.

Flash는 현재 빌드 디렉터리를 직접 사용하지 않고, offline verify를 통과한 패키지만 입력으로
사용합니다. 기존 패키지 디렉터리와 manifest는 덮어쓰지 않습니다. 공식 package는 clean source
commit, 그 commit을 가리키는 annotated tag, `approved` release profile, 같은 commit/profile에서 만든
build receipt를 요구하며, 사용한 profile 파일도 package에 포함합니다.

ESP32 `application-only` 계획은 package의 partition table을 쓰지 않고 보존하므로, profile에 고정된
offset과 로컬 inventory에 기록한 기존 장비의 partition layout ID 및 검증된 partition-table SHA-256이
package와 일치해야 합니다. layout이 달라지면 계획을 거부하고 별도 migration/reprovision 절차를
정의해야 합니다.

현재 manifest에는 전자서명이 없습니다. `offline verify`는 manifest schema와 포함 파일의 크기 및
SHA-256이 서로 맞는지 확인하는 자기 일관성 검사이며, 배포자의 신원이나 패키지 출처를 인증하지
않습니다. 실제 배포 전에는 신뢰할 수 있는 artifact 저장소 또는 별도 전달된 manifest digest와
대조해야 합니다.
