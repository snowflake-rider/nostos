# NOSTOS 저장소 구조와 버전 운영

이 문서는 NOSTOS 펌웨어 저장소의 **구조, 의존성, 버전, 릴리스 절차에 대한 기준 문서**입니다.
명령 사용법은 루트 `README.md`, 타깃별 세부사항은 `firmware/*/README.md`에서 설명합니다.

## 기준 구조

```text
nostos/
├── .gitignore                    로컬 생성물·보존 예외 제외 규칙
├── README.md                     빠른 시작과 일상 명령
├── STRUCTURE.md                  구조·의존성·버전·릴리스 기준(이 문서)
├── AGENTS.md                     작업 절차와 검증 규칙
├── releases/
│   ├── README.md                 릴리스 기록 형식과 보존 원칙
│   ├── index.json                공식 릴리스와 과거 기준 기록의 색인
│   └── baselines/                릴리스가 아닌 과거 검증 기록
└── firmware/                     항상 하나뿐인 활성 펌웨어 소스 트리
    ├── VERSION                   펌웨어 번들 버전
    ├── README.md                 펌웨어 개요
    ├── build.sh                  STM32·ESP32 타깃 빌드 backend
    ├── test-host.sh              STM32·ESP32·protocol 호스트 회귀 검사 backend
    ├── flash-stm32-all.sh        전환기 STM32 배포 도구
    ├── stm32/                    STM32F411 애플리케이션과 HAL
    ├── esp32/                    ESP32-S3 UART/Mesh 브리지
    ├── protocol/                 두 타깃이 공유하는 메시지 계약과 codec
    ├── profiles/release.json     릴리스 도구가 검증하는 빌드·패키지 정책
    ├── inventory/
    │   ├── boards.example.json   값이 없는 로컬 장비 목록 예시
    │   └── boards.local.json     실제 장비 식별자(선택 생성, 로컬 전용, Git 제외)
    ├── tools/
    │   ├── fw                    통합 검사·빌드·패키지·검증 진입점
    │   └── release.py            receipt·package·verify·Flash plan 구현
    └── out/                       빌드·package 시 생성(Git 제외, 평상시 없을 수 있음)
        ├── build-receipts/       commit·profile·toolchain·artifact 연결 기록
        └── releases/             덮어쓰지 않는 로컬 릴리스 패키지
```

`v1/`, `v2/`, `v3/`처럼 전체 소스를 복제한 디렉터리는 만들지 않습니다. `firmware/`가 유일한
활성 소스이며, 과거 버전은 Git tag와 지원 중인 release branch로 복원합니다. 이렇게 해야 STM32,
ESP32, 공통 protocol이 서로 다른 버전의 파일을 우연히 섞어 빌드하는 일을 막을 수 있습니다.

### Canonical Git 구조와 checkout-local 예외

위 트리에서 `boards.local.json`과 `out/` 하위를 제외한 경로가 새 commit에 남길
**canonical Git 구조**입니다. 두 경로는 생성 위치를 고정하기 위해 트리에 표시했을 뿐 Git에
추가하지 않습니다. 하나의 물리 checkout에는 Git에
추가하지 않는 보존물·생성물이 같이 존재할 수 있으며, 이들은 활성 제품 구조의 일부가 아닙니다.

```text
nostos/                         물리 checkout에만 존재할 수 있는 예외
├── .worktrees/                  별도 Git worktree checkout
├── apps/mesh-console/logs/      로컬 현장·테스트 로그
└── docs/
    ├── bluetooth-setting/private-network/Nostos.json
    │                              private Mesh 설정
    └── media/nostos-ride-signals/
                                   로컬 미디어 작업 트리
```

위 예외 경로는 이동·삭제·Git 추가 대상이 아니며, canonical 구조 검사에서는 분리해
판단합니다. 루트의 `apps/`, `docs/`, `experiments/`, `redpill/`, `scripts/`, `tests/`, `tools/`
같은 legacy 디렉터리는 위에 적은 보존 예외 외에는 canonical 구조가 아닙니다. 빈 디렉터리가
물리 checkout에 남아 있어도 Git은 이를 추적하지 않으며, 새 구조의 확장 포인트로 간주하지 않습니다.

## 책임과 의존성

허용하는 소스 의존성은 다음 한 방향뿐입니다.

```text
firmware/stm32 ──┐
                 ├──> firmware/protocol
firmware/esp32 ──┘
```

- `protocol/`은 전송 형식, codec, 경계값과 타깃 독립 테스트를 소유합니다.
- `stm32/`와 `esp32/`는 protocol을 소비하며, 각 타깃의 HAL·ESP-IDF·런타임 연결을 소유합니다.
- `protocol/` 소스나 테스트가 STM32 또는 ESP32 구현 파일을 직접 포함하거나 링크하면 안 됩니다.
  타깃 연결 테스트는 해당 타깃의 `host-tests/`가 소유합니다.
- `profiles/`에는 빌드·패키지 정책만 둡니다. 보드 serial, port, 이미지 hash, Mesh key 같은
  배포별 값은 넣지 않습니다.
- `inventory/boards.local.json`은 개발자 장비에만 두며 Git에 추가하지 않습니다. ESP32의
  `application-only` 계획은 장비 inventory의 partition layout ID와 검증된 partition table SHA-256이
  패키지와 모두 같을 때만 생성합니다.

## 버전은 두 축으로 관리

펌웨어 번들과 통신 protocol은 같은 숫자를 공유하지 않습니다.

| 구분 | 형식 | 의미 |
| --- | --- | --- |
| 펌웨어 번들 | SemVer `MAJOR.MINOR.PATCH` | 함께 검증·배포하는 STM32 + ESP32 산출물의 릴리스 |
| UART protocol | 정수 `1`, `2`, ... | STM32↔ESP32 UART wire contract |
| Mesh protocol | 정수 `1`, `2`, ... | ESP32↔ESP32 Mesh payload contract |

기능을 추가해 번들이 `1.1.0`이 되어도 wire contract가 같으면 protocol은 그대로 둘 수 있습니다.
반대로 기존 수신자가 해석할 수 없는 wire 변경은 해당 protocol 정수를 올리고 호환성 검사와
마이그레이션 계획을 함께 추가해야 합니다. 공식 릴리스 기록에는 번들 버전, UART/Mesh protocol
버전, 양쪽 이미지 hash와 source commit을 모두 기록합니다.

## Git lifecycle

1. `main`은 다음 릴리스를 향하는 단일 활성 소스입니다. 기능 작업은 짧은 feature branch에서
   진행하고 검증 후 `main`으로 합칩니다.
2. 이전 major를 계속 지원해야 할 때만 `release/N.x` branch를 만듭니다. 예를 들어 v2 개발 중
   v1 hotfix가 필요하면 `release/1.x`에서 수정하고, 필요한 수정은 `main`에도 forward-port합니다.
3. 공식 번들을 만들 정확한 commit에는 annotated tag `nostos-vN.M.P`를 붙입니다. package는 tag가
   annotated tag이고 현재 HEAD를 가리키는지 확인합니다. 검증 후 공유한 tag는 이동하거나 재사용하지 않습니다.
4. 지원이 끝난 release branch는 더 이상 변경하지 않아도 tag로 완전한 소스를 복원할 수 있습니다.
   버전별 소스 디렉터리를 추가하지 않습니다.

## 릴리스와 Flash 흐름

```text
doctor → test → build → package → offline verify → flash plan → flash → hardware verify
```

1. **doctor**: 도구 버전, `release.json`, 로컬 환경을 검사합니다.
2. **test**: STM32·ESP32·protocol 호스트 회귀 검사를 수행합니다.
3. **build**: 같은 source commit에서 STM32와 ESP32 산출물을 만들고, commit·profile·toolchain·
   artifact hash를 연결한 build receipt를 Git 제외 영역에 생성합니다.
4. **package**: 빌드 산출물과 hash, 버전, protocol 정보, source commit을 새
   `firmware/out/releases/nostos-vN.M.P/`에 고정합니다. 기존 패키지는 덮어쓰지 않습니다.
   `release` profile이 `approved`이고 작업 트리가 clean이며 같은 HEAD를 가리키는 annotated tag와
   일치하는 clean build receipt가 있을 때만 만들며, 사용한 profile 원본도 패키지 안에 보존합니다.
5. **offline verify**: 장비 없이 manifest schema, 필수 파일과 SHA-256 일치를 검사합니다.
6. **flash plan**: 대상과 패키지, 쓰기 영역, 보존할 상태를 표시하며 장비를 변경하지 않습니다.
   ESP32는 profile에 고정한 bootloader/partition/application offset과 장비 inventory의 partition
   layout 및 partition-table hash를 대조합니다.
7. **flash**: 별도 승인을 받은 대상에 검증된 릴리스 패키지만 씁니다.
8. **hardware verify**: read-back과 필요한 센서·오디오·UART·Mesh 실물 검증을 기록합니다.

Flash 명령은 빌드를 암묵적으로 실행하지 않습니다. 작업 트리의 최신 빌드 결과가 아니라
`offline verify`를 통과한 릴리스 패키지를 입력으로 사용해야 합니다. 빌드 성공이나 read-back 일치만으로
센서, 오디오, 무선 다중 홉 동작이 검증됐다고 판단하지 않습니다.

여기서 패키지를 "고정"하거나 "덮어쓰지 않는다"는 것은 같은 release ID를 재사용하지 않고 로컬
디렉터리를 읽기 전용으로 만든다는 운영 정책입니다. 현재 manifest에는 전자서명이 없으므로
`offline verify`는 패키지 내부 파일과 기록된 SHA-256의 자기 일관성만 확인합니다. 배포 승인 시에는
신뢰할 수 있는 artifact 저장소나 별도로 전달된 manifest digest와 대조해야 하며, 이것을 서명 기반
출처 인증으로 해석하면 안 됩니다.

## Git에 남기는 것과 남기지 않는 것

Git에는 소스, 문서, 릴리스 정책, `releases/index.json`, 릴리스 manifest와 검증 요약을 남깁니다.
공식 배포 바이너리는 로컬 `firmware/out/releases/nostos-vN.M.P/` 또는 별도 artifact 저장소에 보존하고 Git에는
추가하지 않습니다. 로컬 장비 inventory, port, Mesh key, NVS backup, 개인 위치 원본도 Git과 릴리스
manifest에 넣지 않습니다.

`releases/baselines/`는 구조 변경 전 검증 사실을 보존하는 곳이며 공식 릴리스가 아닙니다. baseline의
경로와 hash는 당시 상태를 설명하므로 현재 트리와 달라도 고쳐 쓰지 않습니다.

## 현재 전환기 예외

현재 checkout은 새 운영 체계를 준비하는 단계이며 아직 공식 릴리스가 아닙니다.

- `firmware/VERSION`은 과거 값 `v1`을 그대로 유지합니다. 첫 공식 패키지 전에 정확한 SemVer로
  결정하고 양쪽 타깃의 버전 metadata 영향까지 다시 검증해야 합니다.
- `firmware/profiles/release.json`은 `draft`입니다. source-set·toolchain·실물 검증 기준을 확정한
  뒤에만 `approved`로 변경할 수 있으며, 통합 도구는 draft profile의 공식 package를 거부합니다.
- `nostos-v1.0.0` tag와 공식 v1.0.0 패키지는 아직 없습니다. `releases/baselines/2026-08-29-v1.json`은
  검증 기록일 뿐 공식 릴리스 manifest가 아닙니다.
- protocol v2 소스는 기본값에서 비활성화되어 있지만 현재 빌드 입력에는 포함됩니다. 첫 공식 tag
  전 source-set 정책과 v1/v2 호환 범위를 별도 작업으로 확정해야 합니다.
- 통합 도구의 `fw flash`는 현재 STM32와 ESP32 모두 plan-only입니다. 특히 실제 ESP32 Flash는 아직
  연결하지 않았으며, 실물 Flash는 별도 승인과 구현·검증이 필요합니다.
- 기존 STM32 Flash script에는 실제 쓰기 기능, 배포 장비 식별자와 과거 허용 hash가 남아 있습니다.
  이것은 통합 도구 밖의 전환기 안전장치이며 장기적으로는 로컬 inventory와 검증된 패키지 manifest를
  읽는 명시적 실행 단계로 교체해야 합니다.
