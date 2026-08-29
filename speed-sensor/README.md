# XOSS BLE 속도 센서 연동

이 디렉터리는 사용자가 `XOSS S-26518`로 식별한 속도 센서를 NOSTOS에 연동하기 위한 조사·설계·실물 검증 자료를 모은다. 기존 펌웨어는 이 문서 추가만으로 바뀌지 않는다.

## 목표 경로

```text
XOSS 속도 센서
  └─ BLE GATT notification
       └─ ESP32-S3 GATT Client
            └─ local-only UART 속도 샘플
                 └─ STM32가 NOSTOS_SPEED 패킷 생성
                      └─ 같은 ESP32가 Bluetooth Mesh 중계
                           └─ 다른 ESP32 / STM32 / 표시 장치
```

## 현재 확인된 내용

- NOSTOS 대상은 `firmware/esp32/`의 ESP32-S3, ESP-IDF v5.5.5, Bluedroid 기반 BLE Mesh 펌웨어다.
- 공통 v2 프로토콜에는 `NOSTOS_SPEED(0x40)`과 `valid + kmh_x10` payload가 이미 있다.
- 현재 생성된 `sdkconfig`에는 GATT Client가 켜져 있지만 BLE Mesh와 일반 BLE 스캔을 함께 쓰는 `CONFIG_BLE_MESH_SUPPORT_BLE_SCAN`은 꺼져 있다.
- XOSS 공식 문서는 VORTEX가 표준 ANT+ 및 Bluetooth 프로토콜을 지원한다고 설명한다. 그러나 `S-26518`이라는 표기는 공개된 제품 모델명으로 확인되지 않았다.
- 실제 광고 이름, Service UUID, Characteristic UUID와 notification 원문은 아직 확인하지 않았다.

따라서 우선 [실물 GATT 캡처](GATT-CAPTURE.md)를 수행한다. `0x1816/0x2A5B`가 확인되면 [표준 CSC 규격](CSC-PROTOCOL.md)으로 진행하고, 없으면 제조사 전용 프로토콜을 별도로 분석한다.

## 문서와 템플릿

| 파일 | 용도 |
| --- | --- |
| [GATT-CAPTURE.md](GATT-CAPTURE.md) | 휴대폰으로 광고·서비스·notification을 확인하는 절차 |
| [CSC-PROTOCOL.md](CSC-PROTOCOL.md) | 표준 Cycling Speed and Cadence 패킷과 속도 계산 규칙 |
| [IMPLEMENTATION.md](IMPLEMENTATION.md) | ESP32-S3 구현 단계, 경계 조건, 검증 순서 |
| [CHECKLIST.md](CHECKLIST.md) | 실물 준비부터 완료 판정까지 한 장 체크리스트 |
| [samples/device-observation.example.json](samples/device-observation.example.json) | 장치 식별·GATT 관찰 기록 템플릿 |
| [samples/csc-notifications.example.csv](samples/csc-notifications.example.csv) | notification 원문과 계산 결과 기록 예시 |

## 간이 코드

현재 코드는 실물 BLE 연결 없이 표준 CSC notification 처리 경계만 검증하는 독립 프로토타입이다.

```text
include/xoss_csc.h             CSC decode와 정수 속도 계산 API
include/speed_sensor_local.h   ESP32 → STM32 local-only body 예제
src/                           구현
examples/demo.c                두 CSC notification을 처리하는 실행 예제
tests/test_speed_sensor.c      길이·wraparound·baseline·UART framing 테스트
```

빌드와 실행:

```sh
cmake -S speed-sensor -B /tmp/nostos-speed-sensor-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/nostos-speed-sensor-build --parallel
ctest --test-dir /tmp/nostos-speed-sensor-build --output-on-failure
/tmp/nostos-speed-sensor-build/speed_sensor_demo
```

`speed_sensor_local`의 9-byte body는 구조 설명용 초안이다. NOSTOS Mesh packet이 아니며 `source_id`, `session`, `sequence`를 넣지 않는다. 실제 통합에서는 기존 UART length+CRC framer가 body를 분리한 뒤 STM32가 magic을 먼저 판별하고, `nostos_endpoint_publish(NOSTOS_SPEED)`로 정식 패킷을 만들어야 한다.

## 중요한 경계

- Bluetooth 지원은 Bluetooth Mesh 호환을 뜻하지 않는다. XOSS 센서는 일반 BLE GATT 주변장치이고, NOSTOS ESP32가 Client로 연결한 뒤 값을 Mesh 메시지로 변환해야 한다.
- 센서의 Bluetooth 주소만으로 제품 모델이나 프로토콜을 추측하지 않는다.
- 한 센서가 동시에 허용하는 BLE 연결 수는 실물로 확인한다. XOSS 앱이 연결된 상태에서는 ESP32 연결이 거부될 수 있다.
- 정확한 속도를 위해 실제 타이어의 1회전 이동거리인 `wheel_circumference_mm`가 필요하다.
- Flash, reset, provisioning, Mesh 키 변경은 이 디렉터리의 문서 작업 범위가 아니다.
- 간이 코드는 표준 CSC를 가정한 호스트 코드일 뿐이며, XOSS 실물 GATT 지원이나 BLE Mesh 공존을 검증한 결과가 아니다.

## 참고 자료

- [XOSS VORTEX 공식 영문 설명서](https://www.xoss.co/static/manuals/XOSS_Vortex_EN.pdf)
- [XOSS 센서 연결 안내](https://support.xoss.co/hc/support/articles/1687674697-connect-the-sensor-to-sprint)
- [Bluetooth SIG Cycling Speed and Cadence Service 1.0](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/CSCS_v1.0/out/en/index-en.html)
- [Espressif GATT Client 예제 설명](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_client/tutorial/Gatt_Client_Example_Walkthrough.md)
- [Espressif BLE Mesh 일반 BLE Scan 공존 설정](https://docs.espressif.com/projects/esp-idf/en/release-v5.0/esp32s3/api-reference/kconfig.html#config-ble-mesh-support-ble-scan)
