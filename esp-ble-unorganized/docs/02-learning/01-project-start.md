# 01. 프로젝트 시작과 첫 빌드

[학습 순서](README.md) · [전체 시작 메뉴](../../README.md)

**목표: 새 ESP32-S3 프로젝트를 만들고 Mac에서 첫 빌드를 해 본다. 보드에 기록하는 것은 다음 단계다.**

## 시작 전

- ESP-IDF와 개발 도구가 이미 설치되어 있어야 한다. 아래 SDK 경로는 현재 Mac 기준이다.
- 새 프로젝트를 둘 상위 폴더에서 시작한다. 그 안에 `my_ble` 폴더가 생성된다.
- 기존 프로젝트가 아닌 **새 프로젝트용 절차**다. `my_ble`라는 기존 폴더가 비어 있지 않으면 다른 이름을 사용한다.

## 1. 개발 환경 활성화

터미널에서 ESP-IDF 도구를 사용할 수 있게 한다.

```bash
source /Users/kafka/esp/esp-idf-v5.5.5/export.sh
```

## 2. 프로젝트 생성 후 이동

기본 소스와 CMake 설정을 자동으로 만든다.

```bash
idf.py create-project my_ble
cd my_ble
```

### 프로젝트 생성 직후의 구조

현재 설치된 ESP-IDF v5.5.5 기준이다.

```text
my_ble/
|-- CMakeLists.txt          # 프로젝트 전체 빌드 설정
`-- main/
    |-- CMakeLists.txt      # 컴파일할 소스 파일 지정
    `-- my_ble.c            # 우리가 동작을 작성할 C 코드
```

`my_ble.c`의 `app_main()`은 처음에는 비어 있다. 아직 BLE나 실행 확인용 로그는 없다.

## 3. 사용할 칩 지정

ESP32-S3용으로 설정한다. 이 과정에서 `sdkconfig`가 생성된다.

```bash
idf.py set-target esp32s3
```

주의: 기존 프로젝트에서 실행하면 빌드 폴더와 설정을 초기화한다. 여기서는 방금 만든 `my_ble` 안에서만 실행한다.

## 4. 빌드

소스와 설정을 이용해 펌웨어 파일들을 만든다.

```bash
idf.py build
```

### 칩 지정과 첫 빌드 후의 구조

새로 생기는 `sdkconfig`와 `build/`를 포함한 예시다. 주요 파일만 표시했다.

```text
my_ble/
|-- CMakeLists.txt
|-- sdkconfig                  # 현재 칩과 기능 설정
|-- main/
|   |-- CMakeLists.txt
|   `-- my_ble.c
`-- build/                     # 자동 생성되는 빌드 결과
    |-- my_ble.bin             # 보드에 기록할 애플리케이션
    |-- my_ble.elf             # 실행 코드와 디버깅 정보
    |-- bootloader/
    |   `-- bootloader.bin     # 앱 실행을 준비하는 부트로더
    `-- partition_table/
        `-- partition-table.bin # Flash 메모리 구역 정보
```

오류 없이 끝나고 `build/`에 결과 파일이 생겼는지 확인한다. **여기까지는 Mac에서 파일을 만든 것일 뿐, 보드에 설치하거나 실행한 것은 아니다.**

## 다음

[02. Hardware Bring-up — 보드에서 첫 실행 확인](02-hardware-bringup.md)

참고: [빌드에 필요한 파일과 sdkconfig 살펴보기](../03-reference/BUILD-AND-BOOT.md) · [전체 학습 순서](README.md)

근거: [공식 create-project·set-target·build 설명](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/tools/idf-py.html). 이번 작업에서는 문서만 작성했으며 위 프로젝트 생성·설정·빌드 명령은 실행하지 않았다.
