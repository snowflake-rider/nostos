# NOSTOS 작업 지침

- 수정할 파일만 읽는다. 펌웨어 명령은 `firmware/README.md`, 배선은 `PINS.md`, protocol 변경은
  `firmware/protocol/` 문서, 구조 변경은 `STRUCTURE.md`가 필요할 때만 읽는다.
- 저장소 전체 문서·Linear·이력을 선조회하지 않는다. 필요한 사실은 실제 설정과 코드에서 확인한다.
- 반복 개발은 영향받은 대상만 빠르게 검사·증분 빌드한다.
  - `bash firmware/tools/fw check <stm32|esp32|protocol>`
  - `bash firmware/tools/fw build <stm32|esp32>`
- 전체 `fw test`와 `fw release-build`는 사용자 요청, 공통 프로토콜 변경, 릴리스 때만 실행한다.
- 독립적인 큰 작업이 둘 이상이면 2~3개 subagent를 병렬로 쓴다. 정확한 담당 경로와 검증 명령을 넘기고,
  각 subagent는 루트 문서 재탐색 없이 담당 파일만 읽으며 한 파일에는 writer 한 명만 둔다.
- 자동 commit/push, hook, 백그라운드 동기화를 만들지 않는다.
- Flash는 한 보드와 명령을 dry-run으로 확인한 뒤 사용자 승인 후 실행한다.
- erase, reset, provisioning, Mesh 키 변경, 비밀정보 접근은 별도 승인 없이는 하지 않는다.
