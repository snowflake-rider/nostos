# NOSTOS Firmware

STM32F411RE는 sensor/button producer와 OLED·RGB·audio actuator를 담당하고, ESP32-S3가 공식 message
생성·검증·상태·task scheduling과 Bluetooth Mesh를 단일 소유합니다. 이 디렉터리는 현재 활성 소스 한
벌이며 기본 wire protocol은 v2입니다. 구조·버전 정책은 [STRUCTURE.md](../STRUCTURE.md)를 따릅니다.

## 명령

```sh
bash firmware/tools/fw doctor
bash firmware/tools/fw check stm32
bash firmware/tools/fw check esp32
bash firmware/tools/fw build stm32
bash firmware/tools/fw build esp32
bash firmware/tools/fw test
bash firmware/tools/fw release-build all
bash firmware/tools/fw package --version X.Y.Z
bash firmware/tools/fw verify --release nostos-vX.Y.Z
bash firmware/tools/fw flash --release nostos-vX.Y.Z --target stm32 --node rider-1 --dry-run
```

`check`와 `build`는 캐시를 재사용하며 장비나 release receipt를 변경하지 않습니다. ESP32 build와 Flash에는
ESP-IDF v5.5.5가 필요합니다. 전체 회귀는 `test`, 공식 package용 receipt는 `release-build`에서만 만듭니다.
Package 절차와 제약은 [릴리스 기록](../releases/README.md)을 따르며 `fw flash`는 검증된 계획만 출력하고
장비에 쓰지 않습니다.

## 단일 보드 개발 Flash

```sh
bash firmware/tools/fw dev --target stm32 --node rider-1 --dry-run
bash firmware/tools/fw dev --target stm32 --node rider-1 --execute
bash firmware/tools/fw dev --target esp32 --node rider-1 --dry-run
bash firmware/tools/fw dev --target esp32 --node rider-1 --execute
```

장비 선택 정보는 Git에서 제외되는 `firmware/inventory/boards.local.json`에 둡니다. `--execute`는 선택한
타깃을 증분 빌드해 한 노드의 application 영역만 쓰고 reset하며 release 검증과 외부 full read-back은
생략합니다. STM32 option bytes·OTP와 ESP32 bootloader·partition table·NVS·provisioning은 보존하고 chip
erase는 하지 않습니다. ESP32는 inventory와 현재 partition layout/hash가 일치해야 합니다. 실제 실행에는
매번 대상과 별도 승인이 필요합니다.

## 경로

- 소스: `firmware/stm32/`, `firmware/esp32/`, `firmware/protocol/`
- 정책·버전: `firmware/profiles/release.json`, `firmware/VERSION`
- 개발 출력: `firmware/stm32/build/Release/nostos_stm32.bin`, `firmware/esp32/build/nostos_esp32.bin`
- 캐시·receipt·package: Git에서 제외되는 `firmware/out/`
- 계약·배선: [protocol](protocol/README.md), [핀](../PINS.md)

키, 장비 serial/port, NVS/Flash 백업은 Git에 넣지 않습니다. 호스트 검사와 빌드·Flash 성공은
센서·오디오·UART·Mesh의 실물 동작 증거가 아닙니다.
