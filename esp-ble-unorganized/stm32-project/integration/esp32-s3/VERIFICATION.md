# 1차 이벤트 bridge 검증 기록

[쉬운 설명](README.md) · [상세 참고](DETAILS.md)

## 먼저 한 줄로 보면

**컴퓨터에서 코드 검사와 빌드는 통과했고, 실제 보드 연결 시험은 남아 있습니다.**

- 확인한 것: 메시지 변환·큐 검사, ESP32 빌드, STM32 빌드.
- 다음에 확인할 것: 버튼 한 번 → 상대 STM32가 같은 메시지를 받는지.

아래는 2026-08-28에 남긴 상세 기록입니다. 이번 문서 수정에서 보드 시험을 새로 한 것은 아닙니다.

기록일: **2026-08-28**. 기준 STM32 소스 revision `3c67d2f`와 동일한 소스/설정으로 빌드했다. 이 기록은 새 ESP32 bridge의 실물 동작을 인증하지 않는다.

## 이번에 확인한 것

| 검사 | 결과 | 범위 |
| --- | --- | --- |
| Host Debug | PASS, 3/3 | codec / bridge / console parser |
| Host Release | PASS, 3/3 | NDEBUG에서도 CHECK 실행 |
| Host ASan + UBSan | PASS, 3/3 | AppleClang, Debug |
| ESP32-S3 / ESP-IDF v5.5.5 | BUILD PASS | 새 firmware, 16MB flash 설정 |
| STM32 Debug, 초음파0 낙차0 | BUILD PASS | 기존 소스 그대로 |
| STM32 Debug, 초음파0 낙차1 | BUILD PASS | 기존 소스 그대로 |
| STM32 Debug, 초음파1 낙차0 | BUILD PASS | 기존 소스 그대로 |
| STM32 Debug, 초음파1 낙차1 | BUILD PASS | 기존 소스 그대로 |
| STM32 Release, 초음파0 낙차0 | BUILD PASS | 기존 소스 그대로 |
| STM32 Release, 초음파0 낙차1 | BUILD PASS | 기존 소스 그대로 |
| STM32 Release, 초음파1 낙차0 | BUILD PASS | 기존 소스 그대로 |
| STM32 Release, 초음파1 낙차1 | BUILD PASS | 기존 소스 그대로 |
| STM32 보존 | PASS | `git diff HEAD -- integration/stm32` 변경 없음 |
| 원본 Layer 7 보존 | PASS | 원본 C/H·CMake·defaults 9개 SHA-256 일치 |

호스트 출력의 주요 기준:

```text
PASS codec: 8 IDs, all 256 byte values, strict length/version/null checks
PASS bridge: shared 32-slot FIFO, reject newest, repeated IDs, wraparound
PASS bridge: no-op/invalid, not-ready discard, 999/1000ms, 64-bit clock
PASS bridge: directions, copied payload/source, self suppression
PASS bridge: exact wire bytes, both API failures, no retries or RX republish
```

ESP32 최종 앱 이미지: `build/bsg_esp32_event_bridge.bin`, `0xe0610` bytes. 앱 partition `0x177000` bytes, 40% 여유. 실제 RAM/Task stack 여유는 런타임에서 별도 확인한다.

첫 ESP32 빌드에서 publication 필드명을 SDK의 `publish_addr`로 고친 뒤 재빌드했다. 최종 ESP32 빌드와 STM32 행렬 로그에서 compiler warning/error는 발견하지 않았다. 이는 전역 정적 분석이나 하드웨어 검증을 의미하지 않는다.

## 재현 위치

호스트 빌드는 저장소 루트에서 [상세 참고의 검사 명령](DETAILS.md#호스트-검사)을 사용한다. 결과 디렉터리: `common/protocol/build/{debug,release,sanitized}`.

ESP32: 이 디렉터리에서 `source /path/to/esp-idf-v5.5.5/export.sh` 후 `idf.py -DIDF_TARGET=esp32s3 build`.

STM32 예시 (`integration/stm32`에서):

```sh
cmake -S . -B build/bridge-check-Debug-u1-f1 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_C_FLAGS=-DFEATURE_ULTRASONIC_SENSOR=1 -DFEATURE_FALL_DETECTION=1'
cmake --build build/bridge-check-Debug-u1-f1
```

Debug/Release와 두 매크로 0/1을 바꾸고 별도 build 디렉터리를 사용해 8조합을 확인했다. 헤더를 수정해 변형을 만들지 않았다. 로컬 `build-*.log`, `Debug-u*-f*.log`, `Release-u*-f*.log`와 build 산출물은 Git 제외다.

## 아직 확인하지 않은 것

| 단계 | 상태 | PASS에 필요한 증거 |
| --- | --- | --- |
| Flash / reboot / app 시작 | NOT_VERIFIED | 보드·포트 식별 후 승인한 Flash와 부팅 로그 |
| 새 Vendor Composition / Bind / Pub / Sub | NOT_VERIFIED | 각 노드 Composition, 실제 AppKey index, C001/TTL7/period0/retransmit0, status |
| 설정 복원 / 삭제 후 readiness | NOT_VERIFIED | 재부팅 복원 및 unbind/key delete/pub 변경 시 송신 거부, 입력 뒤늦은 재생 여부 |
| STM32 TX → ESP32 UART RX | NOT_VERIFIED | 같은 8종 ID와 valid/invalid 카운터, 실제 핀/파형 |
| Mesh RX → STM32 RX/출력 | NOT_VERIFIED | ESP32 source/ID 로그 + STM32 RX/invalid/drop/last ID + 실제 출력 |
| 두 노드 양방향 종단 간 | NOT_VERIFIED | A→B, B→A를 각각 한 건씩 확인 |
| 과부하 / RTOS 동시 실행 | NOT_VERIFIED | full/expired/hw_errors 및 STM32 drop, watchdog·stack 관찰 |
| 세 노드 공유 / controlled Relay | NOT_VERIFIED | 두 수신 STM32 확인, 직접 경로를 차단한 Relay OFF/ON 대조 |

Flash, NVS 삭제, provisioning 변경, 원격 push는 이번 작업에서 수행하지 않았다. 기존 STM32의 한 건 pending 수신·CRC/ACK 부재는 남아 있다. 따라서 현재 결과는 **저속 best-effort 이벤트 경로 시험을 시작할 수 있는 코드/빌드 상태**이며, 완성된 안전 시스템이나 완성된 전체 comm module이 아니다.

낙차 시험은 주입/안전한 시험 장치로 수행한다. 기존 `message_debug_inject`는 remote 처리이므로 로컬 Mesh 송신 시험과 혼동하지 않는다. source별 수치 공유·Periodic Task·대시보드는 다음 단계다.
