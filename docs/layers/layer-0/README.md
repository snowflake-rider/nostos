> 이관 원문: `layers/layer-0/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 0: 가장 작은 Bootloading Workflow

## 목표

이 Layer는 다음 한 줄을 실제 ESP32-S3 serial port에서 받는 것까지 확인한다.

```text
[LAYER-0] RUNTIME_OK count=...
```

`idf.py build` 성공이나 Flash 명령 성공만으로는 PASS가 아니다. Flash 후 새 application이 실행되고 위 marker를 출력해야 최종 PASS다.

## 포함 파일

```text
layer-0/
├── CMakeLists.txt
├── sdkconfig.defaults
├── bootload.sh
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── logs/
    └── README.md
```

ESP-IDF 자체는 vendor SDK이므로 이 디렉터리에 복사하지 않는다. 기본 설치 위치 `/Users/kafka/esp/esp-idf-v5.5.5`를 사용하며, 다른 위치는 `ESP_IDF_PATH`로 지정할 수 있다.

## 한 번에 실행

ESP32-S3 한 대만 USB로 연결한 뒤 실행한다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-0
./bootload.sh
```

여러 serial 장치가 보이면 자동 선택하지 않고 중단한다. 그때는 확인한 ESP32-S3 port를 명시한다.

```bash
./bootload.sh --port /dev/cu.usbmodem5C4C2165221
```

## 자동 실행 단계

```text
1. ESP-IDF와 필수 명령 확인
2. USB serial port 존재/개수 확인
3. macOS IORegistry USB profile 저장
4. ESP32-S3 ROM chip_id 확인
5. 16 MB SPI flash profile 확인
6. esp32s3 target 설정
7. bootloader + partition table + application build
8. 현재 보드에 Flash하고 hard reset
9. serial에서 [LAYER-0] RUNTIME_OK 확인
10. RESULT=PASS 또는 RESULT=FAIL 기록
```

## 로그 이름

매 실행은 별도 파일에 저장된다.

```text
logs/esp32s3-layer-0-bootload-20260827T153045-KST.log
```

실패하면 script는 실패한 `STAGE`, 원인 메시지, `RESULT=FAIL`을 남기고 0이 아닌 exit code로 종료한다.

## 주의

- 이 script는 연결된 보드의 현재 application을 Layer 0 application으로 교체한다.
- `erase-flash`는 실행하지 않는다.
- `layers`의 각 디렉터리는 독립 firmware다. 여러 Layer가 동시에 설치되는 구조가 아니다.
- `BOOT_SUCCESS`는 최초 `app_main()` 진입 표시이고, 반복되는 `RUNTIME_OK`는 script가 늦게 serial port를 열어도 실행을 검증하기 위한 표시다.
