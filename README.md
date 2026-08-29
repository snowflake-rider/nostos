# NOSTOS

NOSTOS의 STM32F411RE 애플리케이션과 ESP32-S3 Bluetooth Mesh bridge를 한 저장소에서 관리합니다.

이 저장소에는 **현재 개발 중인 펌웨어 소스 한 벌만** 둡니다. `v1/`, `v2/`, `v3/`처럼 전체 소스를 복사하지 않고, 과거 버전은 Git tag와 필요한 기간만 유지하는 release branch, 덮어쓰지 않는 release package로 보존합니다. 현재 기본 설정은 v1 wire protocol입니다.

구조·의존 방향·버전·릴리스 수명 주기의 기준은 [STRUCTURE.md](STRUCTURE.md)입니다.

```text
.
├── .gitignore            로컬 생성물·보존 예외 제외 규칙
├── README.md             빠른 시작과 일상 명령(이 문서)
├── STRUCTURE.md          구조와 버전 관리의 단일 기준 문서
├── AGENTS.md             작업·검증·장비 안전 규칙
├── releases/             추적하는 릴리스 색인·과거 baseline 기록
└── firmware/             현재 STM32·ESP32·protocol 소스와 통합 도구
```

실제 checkout에는 Git에 추가하지 않는 worktree·현장 로그·private 설정·미디어 보존물이
남아 있을 수 있습니다. 이들은 활성 소스 구조가 아니며 [checkout-local 예외](STRUCTURE.md#canonical-git-구조와-checkout-local-예외)에서 별도로 정의합니다.

## 시작하기

```sh
bash firmware/tools/fw doctor
bash firmware/tools/fw test
bash firmware/tools/fw build stm32
```

ESP32 빌드는 ESP-IDF v5.5.5 환경을 활성화한 뒤 실행합니다.

```sh
bash firmware/tools/fw build esp32
```

빌드와 호스트 테스트는 장비를 변경하지 않습니다. Flash는 빌드 트리가 아니라 SHA-256으로 고정된 release package만 입력으로 사용하며, 현재 통합 도구의 Flash 명령은 계획 확인용 dry-run만 지원합니다.

세부 펌웨어 동작과 배선은 [firmware/README.md](firmware/README.md)를 참고하세요.
