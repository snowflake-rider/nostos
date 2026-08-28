> 이관 원문: `docs/02-learning/02-hardware-bringup.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 02. Hardware Bring-up — 보드에서 첫 실행 확인

[학습 순서](README.md) · [전체 시작 메뉴](../04-records/esp-ble-original-index.md)

**목표: ESP32-S3에 최소 펌웨어를 넣고, `Firmware started!` 로그로 실제 실행을 확인한다.**

[01번 문서](01-project-start.md)에서 만든 `my_ble` 프로젝트를 이어서 사용한다.

```text
로그 코드 작성 -> Build -> 보드·포트 확인 -> Flash -> 부팅·로그 확인
```

## 1. 로그 코드 작성하기

`my_ble/main/my_ble.c`를 열고 다음 내용을 작성한다. 처음에는 별도 소스나 헤더 파일을 추가하지 않는다.

```c
#include "esp_log.h"

void app_main(void)
{
    ESP_LOGI("APP", "Firmware started!");
}
```

- `esp_log.h`: ESP-IDF의 로그 출력 기능을 사용하기 위한 헤더.
- `app_main()`: ESP-IDF의 시작 준비가 끝난 뒤 호출되는 우리 앱의 시작 함수.
- `ESP_LOGI(...)`: `APP`이라는 태그로 안내 로그를 출력한다.

소스 파일명을 그대로 사용하므로 이번에는 CMake 설정을 바꿀 필요가 없다. 기본 INFO 로그 설정을 사용하는 예제다.

## 2. 다시 빌드하기 — Build

ESP-IDF 환경이 활성화된 터미널에서 프로젝트 최상위 `my_ble` 폴더로 이동해 실행한다. 새 터미널이라면 01번의 `source .../export.sh`를 먼저 실행한다. 아래 Flash·Monitor 명령도 이 폴더에서 실행한다.

```bash
idf.py build
```

**여기까지는 수정한 펌웨어 파일을 만든 단계다. 아직 보드 실행을 확인한 것은 아니다.**

## 3. 보드 연결과 포트 확인하기

ESP32-S3 보드 한 대를 데이터 전송이 가능한 USB 케이블로 Mac에 연결한다.

```bash
ls /dev/cu.*
```

연결 전후의 목록을 비교해 해당 보드의 포트를 확인한다. 예를 들어 `/dev/cu.usbmodem…` 또는 `/dev/cu.usbserial…`처럼 표시될 수 있다.

**아래 명령의 `PORT`는 방금 확인한 실제 포트 경로로 바꾼다.** 보드에 USB 단자가 둘이라면 보드 설명에서 Flash·시리얼 콘솔에 사용하는 단자를 확인한다.

## 4. 보드에 기록하기 — Flash

주의: 대상 보드에 있던 프로그램이 새 펌웨어로 바뀐다. 기록할 보드와 포트가 맞는지 먼저 확인한다.

```bash
idf.py -p PORT flash
```

프로젝트 설정에 따라 부트로더, 파티션 테이블, 앱을 Flash 메모리에 기록한다. 필요한 경우 빌드도 자동으로 진행한다. [공식 Flash 명령 설명](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/tools/idf-py.html#flash-the-project-flash)

**Flash 성공은 기록 완료다. 우리 앱이 실행되는지는 다음 단계에서 확인한다.**

## 5. 부팅과 실행 로그 확인하기 — Boot & Check

```bash
idf.py -p PORT monitor
```

보드가 보내는 로그를 읽는다. 확인할 문구는 다음과 같다.

```text
Firmware started!
```

실제 로그에는 레벨, 시간, `APP` 태그가 함께 붙는다. 이 코드는 부팅 때 한 번만 출력하므로, 놓쳤다면 **모니터가 연결된 상태에서 보드의 RESET/EN 버튼을 눌러** 다시 확인한다.

모니터 종료: `Ctrl + ]`. [공식 Monitor 사용법](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-guides/tools/idf-monitor.html)

로그가 없다면 콘솔 설정과 연결한 USB 포트가 맞는지도 확인한다. 로그가 없다는 사실만으로 Flash 실패라고 단정하지 않는다.

## 완료 기준과 다음 단계

**방금 기록한 대상 보드에서 `Firmware started!`를 확인하면, 이번 기초 Bring-up의 목표를 달성한 것이다.** 부트로더 로그만 보이는 것으로는 충분하지 않다.

여기서는 최소 앱의 실행까지만 확인한다. BLE 통신이나 모든 하드웨어 기능을 검증한 것은 아니다. 그다음은 [03. BLE Advertising — 스마트폰에서 보드 발견하기](03-ble-advertising.md)로 이어간다.

이해 확인: `idf.py build`가 성공했는데 아직 보드에서 실행됐다고 말할 수 없는 이유는?

[전체 학습 순서](README.md) · [공식 로그 기능 설명](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/log.html#use-logging-library)

이번 작업은 문서 작성이다. 실제 소스 수정·빌드·Flash·시리얼 수신은 수행하지 않았다.
