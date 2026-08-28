# Nostos 팀원 시작 안내

[프로젝트](../../README.md) · [문서 목록](../README.md) · [이관·검사 결과](../04-records/NOSTOS-MIGRATION.md)

## 0. 저장소와 도구 준비

```sh
git clone https://github.com/snowflake-rider/nostos.git
cd nostos
```

비공개 저장소이므로 GitHub 접근 권한이 필요합니다. STM32는 CMake, Ninja, Arm GNU Toolchain(`arm-none-eabi-gcc`)을 PATH에서 찾을 수 있어야 합니다. ESP32는 ESP-IDF **v5.5.5** 환경을 사용합니다. SDK와 도구는 이 저장소에 복사하지 않습니다.

이하 명령은 저장소 루트에서 시작하거나, 표시된 디렉터리로 이동한 상태에서 실행합니다. Windows에서도 각 도구가 설정된 터미널에서 같은 CMake 명령을 사용할 수 있습니다. ESP-IDF는 해당 플랫폼의 ESP-IDF 터미널을 사용합니다.

## 1. 공통 기능과 자기 센서 확인

전원 공통 기능은 버튼, LED, 부저, VS1003B 음성 출력, UART·Mesh 통신입니다. 각 담당자는 기존 [MyApp](../../code/firmware/stm32/MyApp/) 안의 자기 기능을 수정합니다.

- `hw/`: 실제 장치 읽기·제어
- `service/`: 센서 판정, 메시지·출력·통신 처리
- `ap/app.c`: 초기화와 반복 실행 연결
- `audio/`: 음원 원본과 C 배열
- `common/app_config.h`: 센서 사용 여부

[app_config.h](../../code/firmware/stm32/MyApp/common/app_config.h)의 현재 기본값은 `FEATURE_ULTRASONIC_SENSOR=1`, `FEATURE_FALL_DETECTION=1`입니다. 이관에서는 값을 바꾸지 않았습니다. 장착 센서에 맞게 기능 플래그를 확인하고 빌드합니다. 센서 수신 알림과 로컬 센서 측정은 별개이므로 센서를 끄더라도 메시지 수신·출력 코드는 제거하지 않습니다.

새로운 센서별 preset은 아직 없습니다. 서로 다른 보드의 핀 설정을 임의로 공통 `.ioc`에 덮어쓰지 말고, 변경 내용을 먼저 공유합니다.

## 2. STM32 빌드

저장소 루트에서:

```sh
cd code/firmware/stm32
cmake --preset Debug
cmake --build --preset Debug
```

결과는 `build/Debug/nostos_stm32.elf`입니다. Release는 같은 위치에서 다음 명령을 사용합니다.

```sh
cmake --preset Release
cmake --build --preset Release
```

CubeMX 프로젝트는 [nostos_stm32.ioc](../../code/firmware/stm32/nostos_stm32.ioc)입니다. STM32CubeMX에서 코드를 재생성하는 작업은 이번 이관에서 수행하지 않았습니다. 기존 `Core`, `Drivers`, startup, linker script와 `MyApp`을 사용합니다.

## 3. ESP32 빌드

팀 배포 기준은 **기존 Layer 8에서 가져온** [code/firmware/esp32](../../code/firmware/esp32/)입니다. 학습용 `code/layers/layer-8`과 이전 통합본 `code/legacy/esp32-event-bridge`를 팀 배포 경로로 혼동하지 않습니다.

macOS/Linux에서 자신의 ESP-IDF 설치 경로로 환경을 준비합니다.

```sh
export ESP_IDF_PATH="$HOME/esp/esp-idf-v5.5.5"
source "$ESP_IDF_PATH/export.sh"
```

설치 위치가 다르면 첫 줄을 바꿉니다. Windows는 ESP-IDF v5.5.5 터미널을 엽니다. 그 후 저장소 루트에서:

```sh
cd code/firmware/esp32
idf.py build
```

결과는 `build/nostos_esp32.bin`과 `build/nostos_esp32.elf`입니다. 현재 `sdkconfig`를 함께 관리해 이관 시 설정을 보존합니다. `sdkconfig.defaults`는 초기값이며 기존 `sdkconfig`를 덮어쓰지 않습니다. `example_init`은 `IDF_PATH` 아래 ESP-IDF 제공 컴포넌트를 사용합니다.

소스의 UART1은 STM32 바이너리 메시지용, USB Serial/JTAG는 로그·명령용입니다. 네트워크 키·주소·provisioning 상태를 새 보드에 자동 복제하는 프로젝트가 아닙니다.

## 4. 설치와 배선

**빌드만으로 보드의 펌웨어가 바뀌지 않습니다.** 설치는 대상 보드를 확인한 뒤 팀의 기존 STM32/ESP-IDF 다운로드 절차로 별도 수행합니다. 설치 전에 보존해야 할 Flash/NVS/Mesh 설정을 확인하고, 기존 네트워크를 쓰는 보드에 임의의 erase/reset 명령을 실행하지 않습니다.

현재 단방향 검증 배선의 핵심은 다음입니다. 전원을 끄고 [상세 핀 안내](../settings/PIN.md)를 확인합니다.

| 연결 | 핀 |
| --- | --- |
| 외부 시험 버튼 | STM32 D10/PB6 → 버튼 → GND |
| STM32 송신 → ESP32 수신 | STM32 D8/PA9/USART1_TX → ESP32 GPIO18/UART1_RX |
| 공통 접지 | STM32 GND ↔ ESP32 GND |
| 반대 방향 데이터 | ESP32 GPIO17/UART1_TX → STM32 D2/PA10/USART1_RX. 양방향 시험에서 별도 확인 |

GPIO18은 개발보드의 18번째 헤더 핀이라는 뜻이 아닙니다. 각 보드는 USB로 전원을 공급하고 3V3/5V 레일을 서로 연결하지 않습니다. STM32 USART2는 ST-LINK USB 진단용입니다.

## 5. 버튼 송수신 확인

1. 설치한 펌웨어, 실제 배선, Mesh Model/AppKey/그룹 설정을 확인합니다.
2. 버튼을 한 번 누르고 STM32의 정지 요청 `0x13` 송신을 확인합니다.
3. 연결된 ESP32의 `UART_RX`와 `MESH_TX`를 확인합니다.
4. 상대 ESP32의 `MESH_RX`를 확인합니다.
5. 상대 STM32까지 연결했다면 UART 수신과 실제 LED·부저·음성 출력을 별도로 확인합니다.

검증 도구는 [fast-check 설명](../layers/layer-8/FAST_CHECK.md)을 읽고 사용합니다. 새 위치는 `code/firmware/esp32/fast-check.sh`입니다. 도구의 장치 ID와 기대 수신기 목록은 기존 시험 장비 기준이므로 팀원의 보드·현재 연결 대수에 맞는지 먼저 확인해야 합니다. 이번 이관에서는 도구를 실제 serial port에 연결하지 않았습니다.

`MESH_TX api=accepted`는 상대 수신 보장이 아닙니다. [기존 Verified](../verified/README.md)는 D6·76을 사용한 당시 단방향 시험이며, 세 노드 controlled relay나 상대 STM32 출력 검증까지 뜻하지 않습니다.

## 보드 없이 검사하기

저장소 루트에서 공통 프로토콜 검사:

```sh
cmake -S code/common/protocol -B code/common/protocol/build -G Ninja
cmake --build code/common/protocol/build
ctest --test-dir code/common/protocol/build --output-on-failure
```

ESP32 쪽 기존 Layer 8 호스트 검사는 `bash code/firmware/esp32/test-host.sh`로 실행합니다. 문서·저장소 구조 검사는 `python3 code/scripts/check_repository.py`로 실행합니다. 둘 다 보드 Flash를 하지 않습니다.
