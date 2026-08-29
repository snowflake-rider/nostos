# NOSTOS STM32 Layer 2 — Prototype Firmware

NUCLEO-F411RE용 Prototype 완성본입니다. 버튼·USART1·RGB·VS1003B·부저·HC-SR04·MPU6050 캘리브레이션을 한 디렉터리에 묶었습니다. 빌드 이름은 `nostos_stm32`입니다.

이 디렉터리는 이전 MPU6050 캘리브레이션 시제품을 통합·대체하는 독립 빌드 프로젝트입니다. 공통 메시지 계약은 저장소의 `../../libs/protocol/`을 직접 사용하므로 저장소 밖으로 이 폴더만 떼어내는 완전 독립 묶음은 아닙니다. `BUTTON_OUTPUT_TEST`가 아니라 기본 Prototype(v1) 구성이 기준입니다.

## 빌드

Arm GNU Toolchain, CMake, Ninja를 PATH에 준비합니다.

```sh
cd firmware/stm32-layer2
cmake --preset Debug
cmake --build --preset Debug
```

Release는 같은 위치에서 `cmake --preset Release`, `cmake --build --preset Release`로 빌드합니다. CubeMX 입력은 [nostos_stm32.ioc](nostos_stm32.ioc)입니다. `Core/`, `Drivers/`, `MyApp/`의 기존 경계를 유지합니다.

## 테스트와 문서

- 저장소 루트에서 `bash firmware/stm32-layer2/tools/test-host.sh`: Layer 2 전용 보드 없는 host-tests.
- [팀 시작·배선](../../docs/getting-started/README.md)
- [SharedState](../../docs/architecture/shared-state.md)
- [버튼·UART 기록](../../docs/verification/stm32-button-uart.md)
- [추가 출력 시험 패치](../../experiments/stm32-output-test/README.md): 현재 펌웨어에 미적용.

저장소 루트의 기존 `tools/test-stm32-host.sh`, 핀 검사와 one-stop 타깃 빌드는 아직 `firmware/stm32/`를 기준으로 합니다. Layer 2 자체 회귀는 반드시 위 전용 스크립트와 이 디렉터리의 CMake 프로젝트로 실행합니다.

## Prototype 버튼 매핑과 장착 자세 캘리브레이션

기본(v1) Prototype 동작은 다음과 같습니다.

- BTN1: `SPEED UP`(0x11), 로컬 초록 RGB + 오디오 + USART1 TX
- BTN2: `SPEED DOWN`(0x10), 로컬 노랑 RGB + 오디오 + USART1 TX
- BTN3: `STOP`(0x13), 로컬 빨강 RGB + 오디오 + USART1 TX
- USART1 RX로 받은 BTN1~BTN3 메시지는 오디오만 재생
- 캘리브레이션 전 기본/`REAR_SAFE` RGB는 OFF이고, 성공 후 `REAR_SAFE`에서만 초록 점등
- 버튼 색상은 2초 뒤 현재 상태로 복귀: 캘리브레이션 전 OFF, 성공 후 `REAR_SAFE`이면 초록
- BTN4 단독 입력은 메시지·RGB·오디오·부저를 만들지 않음
- BTN1 -> BTN2 -> BTN3 -> BTN4를 5초 안에 정확히 입력하면 캘리브레이션을 한 번 시작
- 시퀀스 중 BTN1~BTN3의 일반 동작과 USART1 송신은 그대로 실행
- 틀린 순서 또는 5초 초과 시 시퀀스를 초기화
- 같은 poll에서 둘 이상의 버튼이 동시에 안정화되면 순차 입력으로 인정하지 않음
- 부저는 `FALL_DETECTED`가 확정됐을 때만 시작하며, 버튼·후방 경고·SOS에서는 시작하지 않음
- 50 ms 간격의 안정된 샘플 40개, 즉 약 2초를 연속 수집
- 성공 상태가 `READY`로 전환되면 `calibration_completed.mp3` 안내를 한 번 재생
- 제공받은 원본 음원은 `MyApp/audio/calibration_completed.mp3`에 보존하며, 펌웨어에는 같은 바이트를 `calibration_completed_audio.c` 배열로 포함
- 센서 실패·불안정 timeout·시작 거절에서는 완료 안내를 재생하지 않음
- 자전거가 움직이면 수집한 샘플을 버리고 다시 수집
- 10초 안에 완료되지 않으면 `UNSTABLE` 실패
- 캘리브레이션 전에는 장착 방향을 모르는 상태이므로 MPU6050 낙상 판단을 시작하지 않음
- 재캘리브레이션 실패 시 이전에 성공한 기준값은 보존

캘리브레이션 중에도 메인 루프, UART, Mesh 처리가 멈추지 않도록 `HAL_Delay()`를 사용하지 않습니다. 기준 가속도 벡터와 자이로 영점은 RAM에 저장되므로 재부팅 후 다시 캘리브레이션해야 합니다.

USART2(ST-LINK USB)는 이 변형에서 바이너리 UART 복사 대신 캘리브레이션 텍스트 로그 전용입니다.

D10/PB6 `TEST_BUTTON`은 진단 호환을 위해 `STOP`(0x13)으로 남아 있습니다. Prototype 사용자 입력과 캘리브레이션 시퀀스는 BTN1~BTN4를 기준으로 합니다.

```text
CAL_START
CAL_OK,ax_mg=200,ay_mg=-300,az_mg=933,gx_mdps=200,gy_mdps=-100,gz_mdps=50
CAL_FAIL,UNSTABLE
CAL_FAIL,SENSOR
CAL_REJECT,SENSOR_OR_FALL_STATE
```

보드 없는 전용 검사는 다음처럼 실행합니다.

```sh
bash firmware/stm32-layer2/tools/test-host.sh
```

호스트 검사는 새 버튼 매핑, 정확한 BTN1 -> BTN2 -> BTN3 -> BTN4 시퀀스, 틀린 순서·5초 timeout, 로컬 RGB/오디오, 원격 오디오 전용 출력, FALL 전용 부저, boot-held·tick wraparound, 40개 안정 샘플, 장착 방향 기준 낙상 각도, 센서 실패, 10초 캘리브레이션 timeout과 UART 로그를 다룹니다. 실제 자전거 장착 자세·진동·버튼·USART1/2는 별도 실물 검증 대상입니다.

## SSD1306·DHT11 온습도 화면

`snowflake-rider/stm32-project`의 `940ff2408998d79e181e5b8322ba5e678c038871`
커밋에서 SSD1306·DHT11 드라이버를 가져오고, 현재 Layer 2에 없는 `swarm_state` 의존성은
가져오지 않았습니다. 대신 로컬 DHT11 측정값만 표시하는 작은 서비스로 연결했습니다.

- SSD1306: I2C1 `PB8=SCL`, `PB9=SDA`, 기본 7-bit 주소 `0x3C`
- DHT11: `PA1`, 약 1.2초마다 측정
- 화면: 200ms마다 갱신하며 부팅 직후 `DHT WAIT`, 성공 시 온도·습도와 `DHT OK`,
  실패 시 `DHT ERROR n` 표시
- SSD1306 초기화/갱신 실패 시 2초마다 재시도
- `SSD1306_DISPLAY`, `DHT11_SENSOR` CMake 옵션은 기본 `ON`이며,
  `BUTTON_OUTPUT_TEST=ON`에서는 두 기능을 자동으로 끔
- DHT11의 단일-wire 타이밍 구간은 짧게 IRQ를 막으므로 실물 UART 부하 상태에서
  수신 누락 여부를 별도로 확인해야 함

MPU6050과 OLED는 같은 I2C1을 사용하지만 RTOS 없는 단일 메인 루프에서 순차 호출합니다.
코드·호스트 테스트·ARM 빌드만으로 실제 OLED 주소, 배선, DHT11 풀업과 센서 값을 증명할
수는 없으므로 실물 확인은 Flash 승인 후 진행합니다.

현재 이식본 검증 결과:

- Layer 2 host-tests: Debug/Release/Sanitizer 각각 9/9 통과
- Arm GNU 15.3.1 `-Werror`: Debug, Release, v2 Release, 버튼 진단 Release 통과
- 기본 Release: RAM 3,936 B, Flash/binary 137,632 B
- 기본 Release binary SHA-256:
  `d76589510f81d40f09ac5a31a373481dca448e919d495a7a267f6620eaaf91b0`
- framebuffer: 정적 RAM 1,024 B, 신규 함수의 최대 정적 stack frame 64 B
- 이 binary는 아직 실물 보드에 Flash하지 않음

## 두 보드 플래시

Flash 주소는 STM32F411RE 내부 Flash 시작 주소 `0x08000000`입니다. 전체 chip erase, option byte, OTP는 변경하지 않습니다. 먼저 Release ELF를 binary로 변환합니다.

```sh
arm-none-eabi-objcopy -O binary \
  build/Release/nostos_stm32.elf \
  build/Release/nostos_stm32.bin
```

두 ST-LINK가 동시에 연결되어 있을 때는 반드시 고유 시리얼을 지정합니다.

```sh
st-info --probe

st-flash --serial 066DFF485277504867161930 --reset \
  write build/Release/nostos_stm32.bin 0x08000000

st-flash --serial 066EFF3134584B3043121635 --reset \
  write build/Release/nostos_stm32.bin 0x08000000
```

macOS에서 libusb device capture 권한 경고가 나오면 같은 명령을 `sudo`로 실행해야 합니다. 플래시 뒤에는 각 시리얼을 지정해 Flash를 다시 읽고 원본 binary와 비교하여 검증합니다. ST-LINK Virtual COM Port는 각각 `/dev/cu.usbmodem21102`, `/dev/cu.usbmodem21302`로 식별할 수 있습니다. 포트 이름은 다시 연결하면 달라질 수 있으므로 고유 시리얼을 최종 기준으로 사용합니다.

```sh
st-flash --serial 066DFF485277504867161930 \
  read /tmp/layer2-066D.bin 0x08000000 0x20F44
cmp build/Release/nostos_stm32.bin /tmp/layer2-066D.bin

st-flash --serial 066EFF3134584B3043121635 \
  read /tmp/layer2-066E.bin 0x08000000 0x20F44
cmp build/Release/nostos_stm32.bin /tmp/layer2-066E.bin
```

### 2026-08-29 실제 플래시 기록

아래는 SSD1306·DHT11 이식 전 이미지의 과거 기록입니다. 바로 위의 현재 이식본과
binary 크기·SHA-256이 다르며, 현재 화면 이식본의 실물 배포 증거로 사용하지 않습니다.

- Layer 2 host-tests: Debug 7/7, Release 7/7, Sanitized 7/7 통과
- Arm GNU 15.3.1 타깃 빌드: 기본 v1 Release와 선택형 v2 Release 모두 통과
- Release 설정: `NOSTOS_PROTOCOL_V2=OFF`, `BUTTON_OUTPUT_TEST=OFF`
- ELF: 166,788 B, binary: 134,980 B (`0x20F44`)
- RAM 사용: 2,872 B / 128 KB, Flash 사용: 134,980 B / 512 KB
- binary SHA-256: `594092890103a958bbe42ceb3afea063a0a8023e5dc80b735f79b30dcbf53fa3`
- 원본 완료 음원 SHA-256: `8c41241913272fc2537fcba4165a799641917173a62468794eb7fea40a4e4952`
- `066DFF485277504867161930`: write → 134,980 B read-back → `cmp`/SHA-256 일치 → reset
- `066EFF3134584B3043121635`: write → 134,980 B read-back → `cmp`/SHA-256 일치 → reset
- 마지막 probe에서 두 대상 모두 `STM32F411xC_xE`, Flash 524,288 B로 다시 응답
- 재부팅 직후 캘리브레이션 전 RGB OFF: 사용자 실물 확인 PASS(두 보드)

이 기록은 Flash byte 일치까지만 증명합니다. 버튼·RGB·VS1003B·MPU6050·USART1·Mesh 동작은 아래 순서대로 사람이 확인해야 합니다.

## 실물 확인 순서

1. 재부팅 직후 버튼과 버저가 임의로 동작하지 않는지 확인합니다.
2. BTN1/2/3을 각각 눌러 초록/노랑/빨강 RGB, 로컬 오디오, USART1 송신, 버튼 버저 OFF를 확인합니다.
3. 수신 보드에서 BTN1/2/3 메시지가 오디오만 재생하고 RGB·버저를 바꾸지 않는지 확인합니다.
4. BTN4 단독이 아무 동작도 만들지 않는지 확인합니다.
5. BTN1 → BTN2 → BTN3 → BTN4를 5초 안에 입력하고 `CAL_START`, 약 2초 뒤 `CAL_OK`와 완료 음성을 확인합니다.
6. 실제 낙상 확정 전에는 버저가 울리지 않고 `FALL_DETECTED`에서만 울리는지 확인합니다.

보드 플래시 성공은 센서·오디오·Mesh 동작 성공을 의미하지 않습니다. 위 실물 확인을 별도로 완료해야 합니다.

## v2 프로토콜 선택 경로

공통 v2 codec·UART ISR ring·상태 적용·RGB/부저/오디오 경로를 추가했습니다. CMake `-DNOSTOS_PROTOCOL_V2=ON`으로 선택하며 기본은 기존 v1입니다. v2에서도 부저는 unmuted FALL 사건에만 허용하고 후방 경고·SOS에서는 끕니다. 신뢰된 세션/사건 복구를 담당하는 boot provider가 없으면 v2는 NOT_READY로 시작합니다. [구현 계약과 배포 경계](../../libs/protocol/V2.md) · [호스트 one-stop 테스트](../../tests/message-protocol/README.md).

v2 선택 시 공개 `message_protocol_service_init`을 ELF에 보존해 codec·오디오 경로까지 실제 링크합니다. 이것이 기본 boot hook의 NOT_READY를 해제하지는 않습니다. 저장소 루트의 `bash tests/message-protocol/run.sh --targets`는 설치된 도구로 v2 Debug/Release 및 ESP32-S3를 임시 복사본에서 빌드하고, 링크된 경로도 검사합니다. Flash하거나 보드 설정을 바꾸지 않습니다.
