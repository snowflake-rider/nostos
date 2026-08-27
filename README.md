# Bike Swarm Guard

NUCLEO-F411RE 기반 자전거 그룹 주행 안전 프로젝트입니다. 세 팀원이 동일한 통합 STM32 펌웨어를 기준으로 개발하며, 버튼 요청과 센서 이벤트를 공통 메시지로 변환해 LED, 부저, 음성 및 USART 통신에 사용합니다.

## 공용 펌웨어

세 보드가 사용하는 기준 프로젝트는 [`integration/stm32`](integration/stm32)에 있습니다.

```text
integration/stm32/
├─ Core/                 # STM32CubeMX 생성 코드
├─ Drivers/              # CMSIS 및 STM32 HAL
├─ MyApp/
│  ├─ ap/                # 전체 애플리케이션 실행
│  ├─ audio/             # MP3와 C 배열 음원
│  ├─ common/            # 기능 설정과 메시지 ID
│  ├─ hw/                # 버튼, LED, 부저, 센서, VS1003B
│  └─ service/           # 메시지, 안전 판단, 오디오, USART
├─ tools/                # 음원 변환 도구
├─ bike_swarm_guard.ioc
└─ CMakeLists.txt
```

## 구현된 기능

- 버튼 4개 요청: 감속, 가속, 안전·응원, 정지
- RGB LED 상태 표시
- 액티브 부저 경고
- VS1003B MP3 음성 출력
- MPU6050 낙차 감지 및 10초 확인 시간
- HC-SR04 50cm 후방 경고(선택 기능)
- USART1 115200bps 메시지 송수신
- 로컬 메시지 출력 및 상대 보드 1회 전송
- 수신 메시지 재전송 방지

## 시작하기

ESP32 Mesh 연결은 [1차 이벤트 통합 설계](docs/superpowers/specs/2026-08-28-stm32-mesh-event-bridge-design.md)에서 준비합니다. 기존 STM32 이벤트를 먼저 연결한 뒤 센서 수치 공유를 확장하는 순서이며, 현재는 설계 검토 단계입니다. 아래 기존 USART 검증 결과는 Mesh 통합 성공을 뜻하지 않습니다.

1. 저장소를 clone합니다.
2. STM32CubeMX에서 `integration/stm32/bike_swarm_guard.ioc`를 엽니다.
3. STM32Cube 확장 또는 CMake로 `integration/stm32`를 빌드합니다.
4. NUCLEO-F411RE에 펌웨어를 다운로드합니다.

빌드 결과물은 Git에 포함하지 않습니다. CubeMX 설정과 배선은 [CUBEMX_SETUP.md](CUBEMX_SETUP.md), 폴더 역할은 [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md), USART 배선은 [common/PIN_ASSIGNMENT.md](common/PIN_ASSIGNMENT.md)를 확인하세요.

## 기능 설정

`integration/stm32/MyApp/common/app_config.h`에서 센서 기능을 선택할 수 있습니다.

```c
#define FEATURE_ULTRASONIC_SENSOR 1
#define FEATURE_FALL_DETECTION 1
```

값이 `1`이면 사용하고 `0`이면 해당 센서 접근과 메시지 생성을 중지합니다. 현재 공용 기본값은 두 센서 모두 사용입니다.

## 현재 검증 상태

- 기존 버튼, LED, 부저, VS1003B 동작 확인
- 두 STM32 사이 USART 1바이트 메시지 송수신 확인
- 센서 보드의 낙차·후방 이벤트를 출력 보드에서 수신 확인
- 통합 프로젝트 초음파 ON/OFF 빌드 확인
- 한 보드 전체 센서 배선 및 두 통합 보드 연결 시험 진행 중

세부 시험 항목은 [integration/README.md](integration/README.md)에 기록합니다.
