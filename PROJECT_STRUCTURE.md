# 프로젝트 폴더 구조

이 문서는 저장소의 폴더 용도와 팀원별 작업 범위를 설명한다.

## 전체 구조

```text
stm32-project/
├── README.md
├── PROJECT_STRUCTURE.md
├── common/
│   ├── README.md
│   └── stm32/
│       ├── include/                 # 공통 헤더
│       └── src/                     # 공통 구현
├── modules/
│   ├── README.md
│   ├── 01-sensor-module/
│   │   ├── README.md
│   │   └── stm32/                   # 팀원 1의 전체 CubeMX 프로젝트
│   ├── 02-sensor-module/
│   │   ├── README.md
│   │   └── stm32/                   # 팀원 2의 전체 CubeMX 프로젝트
│   └── 03-communication/
│       ├── README.md
│       ├── stm32/                   # UART 시험용 CubeMX 프로젝트
│       ├── esp32-s3/                # ESP-IDF 통신 펌웨어
│       ├── pico2/                   # Pico 2 WH 비교 펌웨어
│       └── docs/                    # 배선과 시험 결과
└── integration/
    └── README.md                    # 세 모듈 통합 시험
```

## 각 STM32 프로젝트

각 모듈의 `stm32/`에는 STM32CubeMX가 생성한 전체 프로젝트를 넣는다. 세 보드가 모두 NUCLEO-F411RE이더라도 센서와 핀 설정이 다르므로 프로젝트를 하나로 공유하지 않는다.

```text
stm32/
├── Core/
├── Drivers/
├── cmake/
├── CMakeLists.txt
├── CMakePresets.json
├── module-name.ioc
├── STM32F411xx_FLASH.ld
└── startup_stm32f411xe.s
```

`.ioc`, `Core`, `Drivers`, CMake 설정, 시작 파일과 링커 스크립트는 Git에 포함한다. `build`, `Debug`, `Release`, `.elf`, `.hex`, `.bin` 같은 빌드 결과물은 포함하지 않는다.

## 공통 코드와 개별 코드

| 구분 | 위치 | 예시 |
|---|---|---|
| 세 STM32가 함께 사용 | `common/stm32` | UART 패킷, 메시지 파싱, 노드 설정, 오류 코드 |
| 특정 센서만 사용 | 각 모듈의 `stm32` | DHT11, MPU6050, 초음파 센서 드라이버 |
| 세 통신 보드가 사용 | `modules/03-communication/esp32-s3` | UART 수신, BLE, Bluetooth Mesh |
| 비교 시험 | `modules/03-communication/pico2` | Pico 2 WH BLE·Mesh 시험 |

공통 코드는 여러 사람이 동시에 직접 수정하지 않는다. 변경이 필요하면 팀에 알리고 한 사람이 수정한 뒤 다른 모듈에서 확인한다.

## 담당 범위

| 담당 | 주 작업 위치 |
|---|---|
| 팀원 1 | `modules/01-sensor-module/` |
| 팀원 2 | `modules/02-sensor-module/` |
| 팀원 3 | `modules/03-communication/` |
| 팀 공통 | `common/`, `integration/` |

01번과 02번 모듈의 주제가 정해지면 폴더 이름을 실제 모듈 이름으로 바꾼다.
