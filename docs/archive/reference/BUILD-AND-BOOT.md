> 2026-08-28 정리: 이 문서의 Layer 0–7 소스 링크는 삭제 직전 Git 스냅샷을 가리킵니다. 학습 프로젝트는 현재 작업 트리에서 제거했으며, 아래 명령과 결과는 과거 기록입니다.

> 이관 원문: `docs/03-reference/BUILD-AND-BOOT.md`. 현재 실행 경로는 [팀원 시작 안내](../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Build & Boot — 빌드 파일과 부팅 개념

[전체 시작 메뉴](../records/esp-ble-original-index.md) · [학습 순서](../learning/README.md)

실습을 처음 시작한다면 [프로젝트 생성과 첫 빌드](../learning/01-project-start.md)부터 읽는다.

**ESP32-S3에 프로그램을 넣고, 실제로 실행되는지 확인하는 과정.**

펌웨어는 보드에서 실행하는 프로그램이다. 블루투스는 장치끼리 무선으로 정보를 주고받는 기술이며, 우리는 BLE를 사용한다.

## 1. 준비하기

Mac에서 ESP-IDF 개발 환경을 준비하고, 보드를 연결한다.

## 2. 만들기 — Build

`idf.py build`로 코드를 펌웨어 파일로 만든다. 아직 Mac에만 있다.

### 어떤 파일로 빌드할까?

현재 Layer 0의 구성이다.

| 파일 | 역할 |
|---|---|
| [CMakeLists.txt](https://github.com/snowflake-rider/nostos/blob/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/layers/layer-0/CMakeLists.txt) | 프로젝트 이름과 ESP-IDF 빌드 설정 |
| [main/CMakeLists.txt](https://github.com/snowflake-rider/nostos/blob/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/layers/layer-0/main/CMakeLists.txt) | 컴파일할 소스와 헤더 검색 경로 지정 |
| [main/main.c](https://github.com/snowflake-rider/nostos/blob/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/layers/layer-0/main/main.c) | 보드에서 실행할 실제 코드 |
| [sdkconfig.defaults](https://github.com/snowflake-rider/nostos/blob/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/layers/layer-0/sdkconfig.defaults) | 프로젝트에서 정한 초기 설정값. 선택 사항 |
| [sdkconfig](https://github.com/snowflake-rider/nostos/blob/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/layers/layer-0/sdkconfig) | 자동 생성되는 현재 프로젝트 설정값 |

`CMakeLists.txt`의 설정에 따라 우리 소스와 필요한 ESP-IDF 코드를 컴파일·링크해 펌웨어를 만든다. 실제 컴파일은 ESP32용 컴파일러가 수행한다.

예를 들어 `main/CMakeLists.txt`에는 다음과 같이 적는다.

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)
```

`SRCS`가 컴파일할 소스를 지정한다. 소스 이름을 `my_app.c`로 바꾸면 이 항목도 맞춰야 한다. `INCLUDE_DIRS`는 헤더 검색 경로다. [공식 빌드 설정 설명](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/build-system.html#minimal-component-cmakelists)

### sdkconfig는 언제 생길까?

**기본값으로 자동 생성되고, 이후 설정 도구로 바꾸는 파일이다.**

1. **기본값 준비:** ESP-IDF의 기본값에 프로젝트의 `sdkconfig.defaults`가 있으면 반영한다.
2. **처음 생성:** `sdkconfig`가 없는 상태에서 처음 설정·빌드할 때 생성한다. `idf.py menuconfig`나 최초 `idf.py build`의 설정 과정 등이 해당한다. 특정 `init` 명령을 꼭 거쳐야 하는 것은 아니다.
3. **나중에 변경:** `idf.py menuconfig`에서 값을 바꾸고 저장한다. 이후 빌드는 저장된 설정을 사용한다. 파일을 직접 편집하기보다 이 도구를 사용한다.

우리 Layer 0의 `sdkconfig.defaults`에는 Flash 크기 **16 MB**, 모니터 속도 **115200** 등이 지정되어 있고, 현재 `sdkconfig`에도 같은 값이 저장되어 있다. 예제 프로젝트가 `sdkconfig`를 이미 포함해서 제공할 수도 있다.

주의: `sdkconfig.defaults`는 초기 기본값이다. 이미 `sdkconfig`에 저장된 값을 나중에 defaults만 수정해서 자동으로 덮어쓰지는 않는다. [공식 설정 파일 설명](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/kconfig/configuration_structure.html)

## 3. 넣기 — Flash

실습은 [02. Hardware Bring-up](../learning/02-hardware-bringup.md)에서 로그 코드 작성부터 Flash·실행 확인까지 이어서 진행한다.

만든 펌웨어를 보드의 Flash 메모리에 기록한다.

## 4. 실행 확인하기 — Boot & Check

보드를 재시작하고, 새 프로그램의 실행 로그를 확인한다.

**여기까지 확인하면 기초 Bring-up 완료. BLE 통신은 다음 단계에서 따로 확인한다.**

[자세한 설명과 기존 이해 기록](../records/BRINGUP-NOTES.md)
