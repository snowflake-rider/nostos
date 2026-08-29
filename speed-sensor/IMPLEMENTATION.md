# ESP32-S3 구현 계획

실물 캡처로 표준 CSC가 확인되기 전에는 이 계획의 펌웨어 단계를 시작하지 않는다.

## 1. 계약 확정

- 센서의 실제 광고 이름과 주소 유형을 확인한다.
- Service `0x1816`, Measurement `0x2A5B`, CCCD `0x2902`를 확인한다.
- notification 주기, 정지 시 동작, disconnect 이유, 동시 BLE 연결 가능 여부를 기록한다.
- 실제 `wheel_circumference_mm`와 stale/정지 timeout 정책을 정한다.
- 센서 값의 NOSTOS `source_id`, session/sequence 소유 주체를 정한다. ESP32 bridge가 UART에서 온 것처럼 위장해 주입하면 안 된다.

## 2. 호스트에서 먼저 구현

하드웨어와 독립적인 작은 모듈로 시작한다.

```text
csc_measurement_decode(raw) -> wheel/crank snapshot
csc_speed_update(previous, current, circumference) -> valid + kmh_x10
```

간이 구현은 다음 파일에 있다.

```text
include/xoss_csc.h
src/xoss_csc.c
include/speed_sensor_local.h
src/speed_sensor_local.c
examples/demo.c
tests/test_speed_sensor.c
```

`speed_sensor_local`은 ESP32에서 계산한 값을 STM32로 전달하는 9-byte local-only body의 초안이다. 정식 NOSTOS header를 흉내 내지 않으며 STM32가 이를 받은 후 endpoint sender로 새 NOSTOS 메시지를 생성해야 한다.

테스트 항목:

- wheel-only, crank-only, wheel+crank
- 짧은 payload, 잘못된 Flags, `delta_ticks == 0`
- event time 16-bit wraparound
- cumulative revolutions 32-bit wraparound
- 첫 샘플 baseline, 정지 timeout, reconnect 후 baseline reset
- `kmh_x10` 반올림과 표현 범위

공통 프로토콜 파일은 기존 `NOSTOS_SPEED` 계약이 충분하면 바꾸지 않는다.

## 3. BLE GATT Client 추가

예상 파일 경계:

```text
firmware/esp32/main/xoss_csc_client.h
firmware/esp32/main/xoss_csc_client.c
```

책임:

- 대상 센서 검색과 명시적 식별
- 연결, 서비스 검색, `0x2A5B` notification 등록, CCCD 활성화
- disconnect 후 제한된 backoff 재검색
- BLE callback에서는 패킷 복사와 queue 전달만 수행
- 별도 task에서 decode, 속도 계산, stale 처리, NOSTOS publish 수행
- 상태, 마지막 수신 시각, malformed/drop/reconnect 횟수를 콘솔에서 확인 가능하게 유지

광고 이름만 일치하면 연결하는 방식은 피한다. 사용자가 확인한 장치 식별 정보와 CSC Service 존재 여부를 함께 검사한다.

## 4. BLE Mesh 공존 설정

현재 설정에는 다음 차이가 있다.

```text
CONFIG_BT_GATTC_ENABLE=y
CONFIG_BT_BLE_42_SCAN_EN=y
# CONFIG_BLE_MESH_SUPPORT_BLE_SCAN is not set
```

ESP-IDF v5.5.5의 정확한 Kconfig와 API를 기준으로 일반 BLE scan 공존을 명시적으로 켜고, generated `sdkconfig`뿐 아니라 재현 가능한 `sdkconfig.defaults`에도 반영한다. Mesh stack이 scan을 소유하는 동안 일반 `esp_ble_gap_start_scanning()`을 독립적으로 경쟁시키지 말고, 해당 IDF 버전이 제공하는 공존 경로를 확인한다.

검증할 자원:

- 부팅 전후 free heap과 minimum free heap
- BLE controller activity 한도
- GATT 연결 중 Mesh 송수신 지연·drop
- Provisioned/Unprovisioned 및 PB-GATT 상태별 동작
- GATT 재연결 폭주가 watchdog이나 Mesh advertising을 굶기지 않는지

## 5. NOSTOS producer 연결

현재 ESP32 v2 runtime은 UART→Mesh와 Mesh→UART bridge이며, `nostos_bridge_accept()`는 `NOSTOS_TO_MESH` 입력의 claimed source가 로컬 source인지 검사한다. 센서 producer를 추가할 때는 다음을 명시적으로 설계한다.

- session과 sequence를 누가 생성·보존하는가
- 로컬 센서 메시지를 기존 bounded queue에 넣을지 별도 bounded queue를 둘지
- 일반 속도 메시지가 FALL/SOS 예약 슬롯을 침범하지 않는지
- Mesh가 준비되지 않았거나 queue가 찼을 때 최신 속도만 유지할지 폐기할지
- 재부팅 후 과거 session/sequence가 재사용되지 않는지

UART 프레임을 가짜로 만들어 기존 입력 경로에 밀어 넣는 방식은 사용하지 않는다.

## 6. 검증 순서

1. CSC parser 단위 테스트와 sanitizer
2. 루트 `bash tools/test-host.sh`
3. `firmware/esp32/`의 `bash test-host.sh`
4. ESP-IDF 환경 및 버전 확인 후 `idf.py build`
5. 한 대의 ESP32-S3에서 기존 NVS/Mesh 설정을 보존한 비파괴 실행 계획 확인
6. 별도 승인 후 Flash
7. 실물 센서 검색→연결→notification→속도 계산
8. GATT 연결 전/중/후 Mesh 송수신 비교
9. 정지, 센서 절전, 범위 이탈, 배터리 제거, 재연결 시험
10. 실제 휠 둘레 기준 속도계와 비교

호스트 테스트와 빌드 성공은 무선·실물 검증 완료가 아니다.
