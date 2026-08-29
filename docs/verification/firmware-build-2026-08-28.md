# 구조 변경 후 펌웨어 전체 빌드 검증

검증일: 2026-08-28 KST. 관련 작업: [KAF-352](https://linear.app/kafkasnowflake/issue/KAF-352).

**실제 MCU 빌드 9개 조합이 모두 통과했습니다.** 컴파일·링크와 ESP32 이미지 생성·파티션 크기를 확인했습니다. 펌웨어 소스·핀·기능·설정 수정은 필요하지 않았습니다. 보드에는 접속하지 않았습니다.

## 결과

| 대상 | 구성 | 결과 | 크기 |
| --- | --- | --- | --- |
| `firmware/stm32` | Debug | PASS | Flash 126,280 / 524,288 B (24.09%), RAM 2,312 / 131,072 B |
| `firmware/stm32` | Release | PASS | Flash 114,344 / 524,288 B (21.81%), RAM 2,320 / 131,072 B |
| `firmware/esp32` — Layer 8 | ESP32-S3, 기존 sdkconfig | PASS | 앱 915,856 B, 앱 파티션 여유 약 40% |
| `experiments/examples/esp32s3/gps-mesh-node` | ESP32-S3, 기존 sdkconfig | PASS | 앱 905,584 B, 앱 파티션 여유 약 41% |
| `experiments/examples/esp32c3/generic-onoff-node` | ESP32-C3, sdkconfig.defaults | PASS | 앱 968,416 B, 앱 파티션 여유 약 37% |
| STM32 출력 시험 패치의 복사본 | Debug/Release × `BUTTON_OUTPUT_TEST=OFF/ON` | PASS, 4/4 | 모든 조합이 Flash/RAM 한도 내 |

- 9개 빌드 로그의 컴파일러·링커 경고와 오류는 **0건**입니다.
- STM32 ELF는 ARM EABI5 hard-float, ESP32-S3 ELF는 Xtensa, ESP32-C3 ELF는 RISC-V 타깃으로 확인했습니다. 호스트 실행 파일을 MCU 빌드로 계산하지 않았습니다.
- ESP32 세 프로젝트의 앱 파티션은 각각 1,536,000 B입니다. 부트로더·파티션 테이블·앱 바이너리가 생성되었고 ESP-IDF 크기 검사가 통과했습니다.
- Layer 8과 GPS의 빌드용 `sdkconfig`는 원본과 바이트 단위로 같습니다. C3는 타깃을 명시하고 기본 설정에서 생성했습니다. 하드웨어의 실제 Flash 용량을 측정한 것은 아닙니다.
- 검증 전후 `firmware/`·`experiments/`의 소스·설정 등 241개 파일 해시가 일치했습니다. 출력 시험 패치는 `build/firmware-verification/patch-source/` 복사본에만 적용했습니다.
- 새 ESP32 빌드 명령에 이전 `nostos/code/`·`esp-ble-unorganized/` 경로 참조가 없습니다. Layer 8에서 `libs/protocol/event_protocol.c`와 `event_bridge.c`가 실제 컴파일되었습니다.
- STM32 RAM 수치는 링커가 배치한 영역의 크기이며 런타임 스택·힙의 최대 사용량 검증은 아닙니다.

## 도구와 환경

| 도구 | 검증한 버전 |
| --- | --- |
| 호스트 | macOS 26.6.2, arm64 |
| Arm GNU Toolchain | 15.3.Rel1, GCC 15.3.1 (20260627) |
| ESP-IDF | v5.5.5, `b774170ff46c393eeb5e495ea37936038d3f4f4f` |
| Espressif Xtensa / RISC-V GCC | `esp-14.2.0_20260121` |
| Python | 3.12.12 |
| CMake / Ninja | 4.4.2 / 1.13.0 |

공식 SDK와 도구는 저장소 밖 `$HOME/.local/share/nostos-toolchains/`에 준비했습니다. 전역 PATH·셸 프로필·시스템 설치 경로는 변경하지 않았습니다. Arm 설치 패키지는 SHA-256 및 ARM Ltd 서명·Apple notarization을 확인한 뒤 로컬 폴더에 풀었습니다. ESP-IDF 태그와 재귀 submodule 상태도 확인했습니다.

SDK 활성화 시 macOS 기본 Bash 3.2의 **자동완성 지원 경고 1건**이 있었습니다. Python 의존성 확인, `idf.py --version` 및 빌드는 모두 정상이며 컴파일러 경고와는 별개입니다.

도구 출처: [ESP-IDF v5.5.5](https://github.com/espressif/esp-idf/releases/tag/v5.5.5), [공식 macOS 설치 안내](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/get-started/linux-macos-setup.html), [Arm GNU Toolchain 배포](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm).

## 이 컴퓨터에서 재실행

새 터미널에서 이 저장소 루트로 이동한 뒤, 해당 터미널에만 도구 경로를 설정합니다.

```sh
export NOSTOS_TOOLS="$HOME/.local/share/nostos-toolchains"
export PATH="$HOME/.local/share/uv/python/cpython-3.12.12-macos-aarch64-none/bin:$NOSTOS_TOOLS/arm-15.3-extracted/Payload/bin:$NOSTOS_TOOLS/build-tools/bin:$PATH"
export IDF_TOOLS_PATH="$NOSTOS_TOOLS/espressif-tools"
export ESP_IDF_PATH="$NOSTOS_TOOLS/esp-idf-v5.5.5"
source "$ESP_IDF_PATH/export.sh"

arm-none-eabi-gcc --version
idf.py --version
```

STM32는 문서화된 두 preset을 그대로 사용했습니다.

```sh
cd firmware/stm32
cmake --preset Debug
cmake --build --preset Debug --parallel 8
cmake --preset Release
cmake --build --preset Release --parallel 8
cd ../..
```

ESP32는 각 프로젝트 디렉터리에서 다음 방식으로 실행했습니다. 아래는 Layer 8 예입니다. GPS도 같은 방식이며, C3는 `cp` 없이 `-D IDF_TARGET=esp32c3`를 사용합니다. 현재 `sdkconfig`를 덮어쓰는 `set-target`은 실행하지 않았습니다.

```sh
cd firmware/esp32
mkdir -p build/firmware-verify
cp sdkconfig build/firmware-verify/sdkconfig
idf.py -B build/firmware-verify \
  -D "SDKCONFIG=$PWD/build/firmware-verify/sdkconfig" \
  -D IDF_TARGET=esp32s3 build
cd ../..
```

출력 시험은 별도 복사본에 [패치](../../experiments/stm32-output-test/changes.patch)를 적용한 뒤, 각 Debug/Release 구성에 `-DBUTTON_OUTPUT_TEST=OFF`와 `ON`을 지정했습니다. 현재 `firmware/stm32/`에는 적용하지 않았습니다. 구성별 정확한 명령은 아래 JSON에 있습니다.

## 증거와 검증 범위

- [기계 판독 결과·산출물 SHA-256·구성별 명령](firmware-build-2026-08-28.json).
- 전체 빌드 로그와 실행 스크립트: `build/firmware-verification/`. 생성물 디렉터리는 Git에서 제외하며, 위 JSON에는 로그·산출물 해시를 남겼습니다.
- STM32 ELF: `firmware/stm32/build/{Debug,Release}/nostos_stm32.elf`.
- ESP32 ELF·BIN: 각 프로젝트의 `build/firmware-verify/`.
- `bash tools/test-host.sh` 재실행: C 검사 54회, Python 검사 25개 및 저장소 경로·문서 링크 검사 PASS.

이전 [이관 검증 기록](../archive/restructure/verification.json)의 `board_firmware_builds: NOT_RUN`은 SDK 설치 전 기록이므로 보존합니다. 이 문서가 그 이후 수행한 MCU 빌드 결과입니다. Mesh Console·Swift·iOS의 앞선 검증도 해당 이관 기록에 별도로 남아 있습니다.

**Flash·erase·reset·provisioning·Mesh 키 변경·시리얼 접속은 하지 않았습니다.** UART·Mesh 전달·센서·오디오·GPS 실물 동작은 이번 빌드로 검증되지 않습니다. Git commit/push도 수행하지 않았습니다.
