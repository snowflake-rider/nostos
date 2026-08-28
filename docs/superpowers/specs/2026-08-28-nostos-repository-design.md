# Nostos 저장소 이관 설계

작성일: 2026-08-28

상태: 이름, 대상 저장소, 코드/문서 분리는 사용자 승인 완료. 아래 구체적인 경로 배치도 사용자가 승인했다. 이후 ESP32 기준은 사용자 지시에 따라 Layer 8로 변경했다. 이 문서 작성만으로 코드 이관·빌드·업로드가 완료된 것은 아니다.

## 1. 목적과 이번 작업 범위

`esp-ble` 안에 있는 STM32·ESP32 펌웨어, 공통 모듈, 학습 예제, iPhone 앱, 도구, 문서를 `snowflake-rider/nostos` 하나에서 관리한다. 최상위에서 빌드·실행하는 코드와 읽는 문서를 분리한다.

- 표시 이름: **Nostos**
- 저장소와 로컬 폴더: `nostos`
- STM32 빌드 이름: `nostos_stm32`
- ESP32 통합 빌드 이름: `nostos_esp32`
- 새로 필요한 C 매크로의 접두사: `NOSTOS_`. 기존 API/메시지 ID를 이름 통일만을 위해 변경하지 않는다.
- 원격: https://github.com/snowflake-rider/nostos
- 원본: `/Users/kafka/Workspace_AI/esp-ble`
- 목적지: `/Users/kafka/Workspace_AI/nostos`

원본 파일, 기존 `stm32-project` 저장소와 원격, 빌드 결과, 보드 상태를 보존한다. 새 목적지에서만 파일 이관과 경로·프로젝트 이름 수정을 수행한다.

센서 기능 재설계, 프로토콜 확장, 속도·온도 전송, RTOS 변경, iPhone 앱 이름/Bundle ID 변경은 이번 이관에 포함하지 않는다. Flash, reset, Mesh 키·provisioning 변경도 하지 않는다.

## 2. 최상위 구조

```text
nostos/
├── README.md                        # 프로젝트 이름과 시작 링크
├── .gitignore
├── code/
│   ├── firmware/
│   │   ├── stm32/                   # CubeMX, HAL, MyApp, CMake, host-tests
│   │   └── esp32/                   # UART ↔ Mesh 통합 ESP-IDF 프로젝트
│   ├── common/protocol/             # 양쪽 펌웨어의 메시지 규칙과 호스트 검사
│   ├── communication-module/        # 기존 별도 C API 실험·검사
│   ├── layers/                      # Layer 0–8 독립 학습 프로젝트
│   ├── examples/                    # ESP32-C3, ESP32-S3 GPS 예제
│   ├── apps/ios-gps-mesh/           # 기존 iPhone 앱과 Swift 검사
│   └── scripts/                     # 환경 확인·모니터 등 실행 도구
└── docs/
    ├── README.md                    # 문서 시작 메뉴
    ├── 00-team/                     # 팀원 시작·빌드·배선·시험 안내
    ├── 01-project/                  # 기존 목표·진행 상태
    ├── 02-learning/                 # 기존 학습 안내
    ├── 03-reference/                # 핀·용어·참고 자료
    ├── 04-records/                  # 사용자 이해·질문·과거 기록
    ├── firmware/                    # STM32/ESP32 설명과 기존 검사 기록
    ├── common/                      # 프로토콜·공통 모듈 설명
    ├── communication-module/        # 기존 API 설명
    ├── layers/                      # Layer별 설명·검증 로그
    ├── examples/                    # 예제 설명
    ├── apps/                        # 앱 설명
    ├── manual/                      # Mesh 구성 등 실물 작업 안내
    ├── settings/                    # 현재 배선 기록
    ├── verified/                    # 실제 시험 결과와 한계
    ├── images/                      # 문서용 원본 이미지
    ├── legacy-stm32/                # 기존 모듈 실험 안내·통합 소개 원문
    └── superpowers/                 # 기존 설계·계획과 이번 이관 문서
```

최상위 README와 라이선스 등 저장소 메타데이터는 예외로 둔다. 실제 설명 문서는 `docs/`, 빌드 입력과 실행 도구는 `code/`에 둔다. 코드에 필요한 MP3·C 배열 음원은 `code/firmware/stm32/MyApp/audio/`에 남긴다. 라이선스와 제3자 소스의 출처 파일은 해당 코드와 함께 보존한다.

빈 실험 디렉터리를 코드로 새로 만들지 않는다. 기존 `stm32-project/modules/`는 현재 설명 문서만 있으므로 그 원문을 `docs/legacy-stm32/modules/`로 이관한다.

## 3. 원본에서 목적지로의 대응

아래 경로는 원본 `esp-ble`과 새 `nostos`를 각각 기준으로 한다. 코드 경로 안의 설명 문서와 관찰 로그는 대응하는 `docs/` 경로로 분리한다.

| 원본 | 목적지 |
| --- | --- |
| `stm32-project/integration/stm32/` | 코드: `code/firmware/stm32/`, 문서: `docs/firmware/stm32/` |
| `stm32-project/integration/esp32-s3/` | 코드: `code/firmware/esp32/`, 문서: `docs/firmware/esp32/` |
| `stm32-project/common/protocol/` | 코드: `code/common/protocol/`, 문서: `docs/common/protocol/` |
| `stm32-project/common/`의 기타 문서 | `docs/common/` |
| `stm32-project/docs/` | `docs/legacy-stm32/docs/` |
| `stm32-project/modules/` | `docs/legacy-stm32/modules/` |
| `stm32-project`의 기타 소개 문서 | `docs/legacy-stm32/`에 원래 상대 경로로 보존 |
| `communication-module/` | 코드: `code/communication-module/`, 문서: `docs/communication-module/` |
| `layers/` | 코드: `code/layers/`, 문서·관찰 로그: `docs/layers/` |
| `examples/` | 코드: `code/examples/`, 문서: `docs/examples/` |
| `apps/` | 코드: `code/apps/`, 문서: `docs/apps/` |
| `scripts/` | `code/scripts/` |
| `docs/` | `docs/`의 기존 상대 경로 유지 |
| `manual/`, `settings/`, `verified/`, `images/` | 각각 `docs/` 아래 같은 이름 |
| 원본 루트 `README.md` | `docs/04-records/esp-ble-original-index.md` |
| 원본 루트 `how-to-make-mesh-network.md` | `docs/04-records/mesh-manual-original-pointer.md` |

이관 시 파일별 원본·목적지·SHA-256·수정 이유를 기록한다. 파일명 충돌은 덮어쓰지 않고 원래 소속을 반영한 경로로 분리한다. 표에 없는 실제 소스·문서가 발견되면 분류한 뒤 포함하며, 조용히 버리지 않는다.

## 4. 유지할 모듈 경계

`MyApp/ap`, `common`, `hw`, `service`, `audio` 구조와 기존 함수 이름을 유지한다.

- 전원 공통: 버튼, LED, 부저, 음성 출력, 메시지 처리, UART, ESP32 Mesh.
- 보드별 선택 기능: 센서 읽기·필터·판정. 기존 기능 플래그와 현재 기본값은 이번 이관에서 바꾸지 않는다.
- 센서가 없는 보드도 원격 이벤트의 수신·알림은 처리할 수 있어야 한다.
- 별도 `communication-module`의 내부 C 자료형을 기존 UART/Mesh wire 규칙으로 자동 대체하지 않는다.
- 기존 SharedState와 수치 전송의 연결 상태를 과장하지 않는다.

## 5. 빌드와 이름 변경

STM32의 CMake 프로젝트명, `.ioc` 파일명과 그 내부 프로젝트명은 함께 `nostos_stm32`로 맞춘다. 핀, 주변장치, ISR, 버튼 코드, 음원, 알고리즘은 바꾸지 않는다. ESP32 통합 CMake 프로젝트명은 `nostos_esp32`로 맞춘다. 학습 Layer·참고 예제·iPhone 앱의 독립 프로젝트 이름은 이번 단계에서 유지한다.

각 프로젝트를 독립적으로 빌드한다. 최상위 CMake 하나로 STM32와 ESP-IDF를 합치지 않는다. `.ioc`, Core, Drivers, startup, linker script, CMake, 필요한 설정과 의존성 명세를 모두 포함한다.

공통 프로토콜은 `code/common/protocol/` 한 곳에서 참조한다. 현재 상대 경로가 유지되는 부분은 그대로 두고, 프로토콜 호스트 검사에서 참조하는 ESP32 `serial_command.c` 등 바뀌는 경로만 수정한다.

실행 스크립트의 프로젝트 경로는 스크립트 위치에서 계산한다. SDK·툴체인은 저장소 밖에서 설치하고 환경변수 또는 PATH로 지정한다. `/Users/kafka`에만 존재하는 경로가 새 팀원 안내의 필수 조건이 되지 않게 한다. 기존 회고·로그의 당시 절대 경로는 역사적 증거로 보존하되 현재 실행 경로와 구분한다.

### ESP32 기준 변경 — 사용자 승인 반영

사용자가 Layer 8을 더 최신 작업본으로 지정하고 팀 배포 기준으로 사용하도록 지시했다. 따라서 `code/firmware/esp32`는 Layer 8의 main·설정·UART 진단·호스트 검사·도구를 기준으로 한다. 공통 codec은 중복 복사하지 않고 `code/common/protocol`을 참조한다. 두 codec C 구현과 메시지 ID는 같고 메시지 헤더의 차이는 설명 주석뿐이다.

이전 통합 원본은 `code/legacy/esp32-event-bridge`에, 해당 문서는 `docs/legacy-stm32/integration/esp32-s3`에 보존한다. 이전 통합본의 빌드 차단 오타 `mesh_node_send_onofSf`는 수정 내역으로 기록한다. 팀 배포 기준은 legacy가 아니다.

새 빌드 이름과 부팅 로그의 project 표시는 `nostos_esp32`로 맞춘다. Layer 8의 UART·Mesh 동작과 설정은 바꾸지 않는다. 과거 실물 시험은 해당 시점의 증거이며 새 빌드의 Flash·무선 수신 완료로 승계하지 않는다.

## 6. Git과 보존

확인 시 새 원격은 비어 있는 비공개 저장소다. 이 설정을 바꾸지 않는다.

기존 Git 저장소는 `esp-ble/stm32-project`이고, 확인한 작업 브랜치는 `feature/stm32-esp32-integration`, HEAD는 `f733599`다. 수정·미추적 파일이 있으므로 커밋된 내용만 가져오면 현재 코드를 잃게 된다.

이관 직전 원본 HEAD, 상태, 파일 목록·해시를 다시 기록한다. 기존 커밋 이력을 새 저장소의 ancestry로 연결하고, 별도 이관 커밋에 현재 작업 파일을 포함한다. 이를 위해 목적지에서만 기존 HEAD를 fetch하고, 기존 이력을 보존하는 merge parent로 연결한 뒤 새 배치의 파일을 기록한다. 원본에서는 checkout, reset, clean, add, commit, remote 변경을 하지 않는다.

중첩 `.git`이나 submodule을 만들지 않는다. 강제 push하지 않는다. 이관과 검사 결과를 확인하기 전 원격에 코드가 배포됐다고 보고하지 않는다.

### 이관에서 제외하되 원본에는 남길 것

- `build*`, Debug/Release, `.build`, DerivedData, managed_components 등 재생성 가능한 빌드·의존성 캐시.
- ELF/BIN/HEX/MAP/오브젝트, `compile_commands.json`, IDE 개인 상태, `.DS_Store`.
- 기존 `.git`, 로컬 에이전트 설정, 임시 파일, 중복 묶음 `layers.zip`.
- 실제 Mesh 키·네트워크 export, 개인 자격증명, private-network 자료. 비공개 저장소라도 커밋하지 않는다.

하드웨어 관찰 로그는 빌드 캐시 로그와 구분하여 `docs/`에 보존한다. 민감값 여부를 검사하고 원문 수정이 필요하면 수정 사실을 표시한다. 테스트 입력 fixture는 코드 쪽에 유지한다. 기존 라이선스는 확장자만 보고 삭제하지 않는다.

## 7. 팀원 문서

문서 시작 순서는 다음으로 고정한다.

1. 공통 기능과 자기 센서 기능 확인.
2. STM32 빌드.
3. ESP32 빌드.
4. 해당 문서의 배선 확인.
5. 버튼 송신과 상대 수신·알림 확인.

이관 단계에서는 현재 설정을 선택·확인하는 방법을 문서화하며, 새로운 센서별 빌드 preset 도입은 다음 작업으로 남긴다. 실제 확인하지 않은 수신·알림 구간은 앞으로 수행할 시험 항목으로 표시한다.

설명 문서의 링크는 새 문서 경로, 소스 링크는 `code/`의 실제 파일로 갱신한다. 기존 사용자 이해·질문, 과거 검증 결과, 한계는 보존한다. 최신 `verified/`의 시험 결과와 이전 STATUS의 미확인 기록이 서로 다른 시점을 나타낸다는 점을 시작 문서에서 설명한다.

## 8. 이관 완료 기준

- 새 저장소에 Git root가 하나만 있고 원격은 지정된 `nostos`다.
- 원본의 모든 이관 대상 파일이 대응표로 추적되며, 내용 변경은 이름·경로·명백한 빌드 차단 수정·새 안내에 한정된다.
- 기존 Git 커밋 이력이 보존되고 원본의 작업 파일 및 Git 상태가 바뀌지 않는다.
- 빌드 산출물·캐시·개인 설정·실제 네트워크 키가 추적되지 않는다.
- 새 문서의 로컬 링크·이미지 경로를 검사하고, 제외한 산출물을 가리키는 과거 증거 링크는 원본 위치 설명으로 구분한다.
- 공통 프로토콜, STM32 호스트 검사, communication-module의 기존 검사를 새 경로에서 실행한다. 지원되는 검사에는 ASan/UBSan도 실행한다.
- `nostos_stm32` Debug/Release와 `nostos_esp32`를 새 빌드 디렉터리에서 빌드한다.
- 코드가 보존되는 Layer/예제/앱도 가능한 범위에서 기존 검사를 실행하고, 실행한 대상과 미실행 대상을 구체적으로 기록한다. 일부만 빌드했다면 전체 빌드 완료라고 쓰지 않는다.
- 어떠한 Flash·보드 reset·Mesh 설정 변경도 수행하지 않는다.
- 결과 보고에서 로컬 이관, 커밋, push, 빌드, 실물 확인을 각각 구분한다.

## 9. 다음 단계

사용자가 경로 배치를 승인했고 이관을 진행한다. 실제 실행 결과는 `docs/04-records/NOSTOS-MIGRATION.md`에 기록한다. 이 설계 문서 자체는 완료 증거가 아니다.
