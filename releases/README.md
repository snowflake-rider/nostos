# NOSTOS 릴리스 기록

구조와 lifecycle은 [`STRUCTURE.md`](../STRUCTURE.md)를 기준으로 합니다. `index.json`은 공식 릴리스와
baseline을 색인하고, `baselines/`는 구조 변경 전 사실을 보존합니다. baseline은 공식 package가 아니며
당시 경로와 hash를 수정하지 않습니다.

## 공식 릴리스

```sh
bash firmware/tools/fw test
bash firmware/tools/fw release-build all
bash firmware/tools/fw package --version X.Y.Z
bash firmware/tools/fw verify --release nostos-vX.Y.Z
bash firmware/tools/fw flash --release nostos-vX.Y.Z --target esp32 --node node1 --dry-run
```

Package는 clean source commit, 그 HEAD의 annotated `nostos-vX.Y.Z` tag, `approved` profile과 같은
commit/profile의 build receipt를 요구합니다. 일반 `fw build`는 개발용이라 receipt를 만들지 않습니다.
기존 package와 manifest는 덮어쓰지 않으며 profile 원본도 포함합니다.

바이너리는 `firmware/out/releases/nostos-vN.M.P/` 또는 별도 artifact 저장소에 두고 Git에 추가하지 않습니다.
장비 serial/port, Mesh key, NVS backup과 개인 위치 원본도 기록하지 않습니다. `offline verify`는 manifest와
포함 파일의 크기·SHA-256 자기 일관성만 검사하므로 출처 인증에는 신뢰 가능한 artifact 저장소나 별도 manifest
digest가 필요합니다.

ESP32 application-only 계획은 partition table과 NVS를 보존합니다. Package와 inventory의 partition layout ID,
partition-table SHA-256이 다르면 Flash 계획을 거부하고 별도 migration/reprovision 절차를 사용합니다.
