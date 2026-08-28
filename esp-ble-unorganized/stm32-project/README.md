# Bike Swarm Guard

자전거를 함께 타는 라이더끼리 버튼·센서 메시지를 공유하는 프로젝트입니다.

**STM32는 센서를 읽고 알림을 처리하고, ESP32는 다른 라이더에게 메시지를 전달합니다.**

통신 부분은 [쉬운 설명](integration/esp32-s3/README.md)부터 읽으면 됩니다. 정지 버튼 하나가 상대에게 전달되는 과정을 예제로 설명했습니다. 코드를 보고 싶다면 [짧은 C 예제](common/protocol/README.md)로 이어집니다.

A·B·C의 센서값을 STM32에 모아 두는 부분은 [SharedState 설명과 예제](integration/stm32/SHARED_STATE.md)를 읽으면 됩니다. 저장·조회 모듈은 준비됐고, 실제 UART 수치 수신과 화면 연결은 다음 단계입니다.

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

ESP32 통신 코드를 만들고 빌드까지 확인했습니다. **실제 보드끼리 메시지가 오가는지는 아직 시험하지 않았습니다.** 지금은 버튼·센서의 사건을 먼저 전달하고, 속도·온도 숫자의 주기 전송은 나중에 붙입니다.

```text
common/protocol/       # 함께 쓰는 메시지 규칙과 큐
integration/stm32/    # 센서 읽기와 LED·음성 출력
integration/esp32-s3/ # 다른 라이더와 메시지 주고받기
```

처음에는 [쉬운 설명](integration/esp32-s3/README.md)과 [C 예제](common/protocol/README.md)만 보면 됩니다. 실제 배선·Mesh 설정 때는 [상세 참고](integration/esp32-s3/DETAILS.md), 시험 결과는 [검증 기록](integration/esp32-s3/VERIFICATION.md)을 확인하세요.

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

## 기존 STM32 검증 기록

- 기존 버튼, LED, 부저, VS1003B 동작 확인
- 두 STM32 사이 USART 1바이트 메시지 송수신 확인
- 센서 보드의 낙차·후방 이벤트를 출력 보드에서 수신 확인
- 통합 프로젝트 초음파 ON/OFF 빌드 확인
- 한 보드 전체 센서 배선 및 두 통합 보드 연결 시험 진행 중

세부 시험 항목은 [integration/README.md](integration/README.md)에 기록합니다.

새 이벤트 Mesh bridge의 검증은 [별도 기록](integration/esp32-s3/VERIFICATION.md)으로 구분합니다. 빌드 성공을 무선 송수신 성공으로 표시하지 않습니다.
