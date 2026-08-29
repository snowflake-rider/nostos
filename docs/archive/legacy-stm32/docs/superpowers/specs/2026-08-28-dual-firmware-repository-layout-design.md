> 이관 원문: `stm32-project/docs/superpowers/specs/2026-08-28-dual-firmware-repository-layout-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# STM32 · ESP32-S3 이중 펌웨어 저장소 구조 설계

작성일: 2026-08-28

상태: 사용자 승인 완료. 이 문서는 디렉터리와 소유권 경계를 확정하며, 아직 파일 이동이나 CMake 변경을 수행하지 않는다.

## 1. 목표

`stm32-project`를 Bike Swarm Guard 전체의 Git repository root로 유지한다. STM32와 ESP32-S3는 같은 제품을 구성하지만 서로 다른 toolchain과 설정으로 빌드되므로, 각각 독립된 project root와 VS Code 창을 사용한다.

```text
repository root
└── 두 독립 펌웨어 + 공유 protocol + 통합 문서
```

합친다는 말은 두 펌웨어 소스를 한 build tree에 섞는다는 뜻이 아니다. 두 펌웨어를 같은 repository에서 version 관리하고, 명시적인 공통 protocol을 통해 연결한다는 뜻이다.

## 2. 확정한 디렉터리 구조

```text
stm32-project/                         # Git repository root
├── README.md                          # 전체 시스템 시작 문서
├── PROJECT_STRUCTURE.md               # 폴더와 소유권 설명
├── common/
│   └── protocol/                      # 플랫폼 독립 통신 계약과 host tests
│       ├── message_type.h             # 양쪽이 공유하는 event ID
│       ├── event_protocol.h
│       ├── event_protocol.c
│       ├── event_bridge.h
│       ├── event_bridge.c
│       └── tests/
├── integration/
│   ├── stm32/                         # STM32 CubeMX/CMake project root
│   │   ├── bike_swarm_guard.ioc
│   │   ├── CMakeLists.txt
│   │   ├── CMakePresets.json
│   │   ├── Core/
│   │   ├── Drivers/
│   │   └── MyApp/
│   │       ├── ap/
│   │       ├── audio/
│   │       ├── common/                # STM32 전용 application 설정
│   │       ├── hw/
│   │       └── service/
│   └── esp32-s3/                      # ESP-IDF project root
│       ├── CMakeLists.txt
│       ├── sdkconfig
│       ├── sdkconfig.defaults
│       └── main/
├── modules/                           # 독립 실험과 학습 자료
└── docs/                              # 전체 설계와 계획
```

현재 `integration/stm32`와 `integration/esp32-s3`의 형제 구조는 유지한다. `firmware/`로 다시 이동하거나 ESP32를 STM32의 `MyApp` 아래로 넣지 않는다.

## 3. 세 가지 root의 의미

| 종류 | 경로 | 책임 |
| --- | --- | --- |
| Repository root | `stm32-project/` | Git, 전체 문서, 공유 코드, 시스템 검증 기록 |
| STM32 project root | `integration/stm32/` | CubeMX 생성, ARM CMake build, ST-LINK debug/flash |
| ESP32 project root | `integration/esp32-s3/` | ESP-IDF configure/build, flash, monitor, Mesh 설정 |

Repository root 하나 아래에 project root가 두 개 존재하는 구성이 정상이다. 각 build 도구를 repository root에 억지로 맞추지 않는다.

## 4. 플랫폼별 소유권

### STM32

STM32는 센서와 버튼을 읽고, application event를 결정하며, 받은 event에 따라 LED·부저·음성을 처리한다. CubeMX 생성 코드, HAL driver와 `MyApp`은 모두 `integration/stm32`가 소유한다.

`MyApp`은 repository 전체의 application 폴더가 아니라 STM32 펌웨어 내부 계층이다. 따라서 ESP-IDF project를 `MyApp` 안에 넣지 않는다.

`MyApp/common/app_config.h`처럼 STM32의 센서 기능을 선택하는 설정은 계속 STM32에 둔다.

### ESP32-S3

ESP32-S3는 STM32가 이미 해석한 event ID를 UART로 받아 Mesh로 전송하고, Mesh에서 받은 event를 로컬 STM32에 전달한다. 센서 reading, calibration, filtering, 위험 판단과 출력 정책을 소유하지 않는다.

ESP-IDF의 `CMakeLists.txt`, `sdkconfig`, `main/`, component dependency는 `integration/esp32-s3`가 소유한다.

### 공유 protocol

양쪽 firmware와 host test가 동일하게 알아야 하는 내용만 `common/protocol`이 소유한다.

- 유효한 event ID
- UART/Mesh wire byte 의미
- encode/decode와 형식 검사
- 플랫폼 독립 queue/bridge 규칙
- host tests

HAL handle, ESP-IDF type, GPIO 번호, RTOS task, 센서 값 보정은 `common/protocol`에 넣지 않는다.

## 5. `message_type.h` 소유권 변경

현재 `message_type.h`는 `integration/stm32/MyApp/common`에 있지만 ESP32와 host protocol code도 직접 사용한다. 현재 ESP32와 host CMake가 STM32 내부 include directory를 참조하므로 의존 방향이 뒤집혀 있다.

```text
현재
ESP32 ────────────────→ STM32/MyApp/common/message_type.h
common/protocol ──────→ STM32/MyApp/common/message_type.h
```

`message_type.h`는 명시적인 1-byte event ID 계약이므로 `common/protocol/message_type.h`로 옮긴다.

```text
변경 후
STM32 ──────┐
            ├──→ common/protocol/message_type.h
ESP32 ──────┤
host tests ─┘
```

이 변경은 event ID 값이나 wire format을 바꾸지 않는다. 기존 8종 ID와 `MSG_NONE`, `MSG_UNKNOWN` 값은 그대로 유지한다. `enum` 객체 자체를 wire로 보내지 않고 기존처럼 명시적으로 `uint8_t` byte를 전송한다.

기존 `2026-08-28-stm32-mesh-event-bridge-design.md`의 `message_type.h` 위치 설명은 이 문서가 승인된 이후의 소유권에 대해서는 본 문서가 우선한다. 구현할 때 기존 설계 문서의 링크도 새 위치로 갱신한다.

## 6. Build와 VS Code 운영

두 펌웨어는 별도 VS Code 창으로 연다.

### STM32 창

```sh
cd stm32-project
code -n --profile STM32 integration/stm32
```

이 창의 CMake source directory는 `integration/stm32`다. STM32Cube CMake, ST-LINK debug와 flash는 이 창에서 수행한다.

### ESP32-S3 창

```sh
cd stm32-project
code -n integration/esp32-s3
```

ESP-IDF shell을 source한 터미널 또는 별도 ESP32 Profile을 사용한다. STM32 Profile 설정을 ESP32 project에 재사용하지 않는다.

### Repository 작업

Git 상태, 전체 문서와 cross-platform 검증은 repository root에서 다룬다.

```sh
cd stm32-project
git status
```

플랫폼별 `build/`는 각 project root 아래에 독립적으로 생성하고 Git에 포함하지 않는다.

## 7. 의존성과 build 규칙

- STM32 CMake는 `common/protocol`의 public header를 include할 수 있다.
- ESP32 component는 `common/protocol`의 필요한 C source와 public header를 포함할 수 있다.
- `common/protocol`은 STM32 또는 ESP32 디렉터리를 include하지 않는다.
- ESP32는 `integration/stm32/MyApp`을 include하지 않는다.
- STM32는 ESP-IDF `main/` 또는 `sdkconfig`를 참조하지 않는다.
- repository root에 두 toolchain을 합친 단일 top-level CMake project를 만들지 않는다.

의존 방향은 다음 한 방향을 지킨다.

```text
integration/stm32   ─┐
                     ├──→ common/protocol
integration/esp32-s3 ┘
```

## 8. 구현 범위

승인된 구조를 반영하는 첫 변경은 아래로 제한한다.

1. `message_type.h`를 `common/protocol`로 이동한다.
2. STM32 CMake include path가 `common/protocol`을 보도록 수정한다.
3. ESP32 component에서 `../../stm32/MyApp/common` include path를 제거한다.
4. host protocol CMake에서 STM32 include path를 제거한다.
5. 새 header 위치를 가리키도록 관련 문서를 갱신한다.

이 변경에서 event ID, UART wire format, Mesh payload, queue 동작, sensor logic, pin 설정과 `.ioc`는 변경하지 않는다.

## 9. 검증 기준

구조 변경은 다음을 각각 독립적으로 통과해야 완료다.

| 계층 | 검증 |
| --- | --- |
| 의존성 | `common/protocol`과 ESP32 CMake에 `stm32/MyApp/common` 참조가 없음 |
| Host | protocol Debug/Release tests와 ASan/UBSan 통과 |
| STM32 | 기존 Debug/Release 및 필요한 feature 조합 build 통과 |
| ESP32-S3 | ESP-IDF `esp32s3` target build 통과 |
| 문서 | 이전 `message_type.h` 경로 참조가 남지 않음 |
| Git | 이동·CMake·문서 파일만 변경되고 build output은 제외됨 |

Build 성공은 flash, boot, UART 수신 또는 Mesh 통신 성공을 뜻하지 않는다. 구조 변경 이후의 hardware 검증 상태는 기존 `VERIFICATION.md` 단계 구분을 유지한다.

## 10. 선택하지 않은 대안

### ESP32-S3를 `MyApp` 아래에 배치

STM32 application 계층과 ESP-IDF project 경계가 섞이고 toolchain 의존 방향이 불명확해지므로 선택하지 않는다.

### `integration`을 `firmware`로 이름 변경

조금 더 일반적인 이름이지만 현재 문서와 CMake 상대 경로를 대량 변경한다. 기능상 이익이 없어 현재 단계에서는 선택하지 않는다.

### 하나의 VS Code 창과 하나의 top-level build

한 창에서 두 toolchain을 동시에 관리하면 Profile, active CMake project와 configure 환경이 섞인다. 사용자가 두 창 운영을 승인했으므로 각 project root를 직접 여는 방식을 사용한다.
