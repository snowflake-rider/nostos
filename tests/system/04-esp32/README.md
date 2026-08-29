# 04. ESP32 빌드

```sh
bash tests/system/04-esp32/run.sh
```

폴더 안에서는 `bash run.sh`. `--json`은 최종 JSON만 출력합니다.

현재 ESP32-S3 소스·sdkconfig와 공통 프로토콜을 결과 폴더에 복사한 뒤 전체 컴파일·링크합니다. 원본 설정·의존성 lock을 변경하지 않습니다.

필요: 설치된 ESP-IDF **v5.5.5**. `ESP_IDF_PATH` / `IDF_TOOLS_PATH`로 위치를 지정할 수 있습니다. `example_init`은 같은 IDF의 로컬 컴포넌트입니다. 자동 설치·Flash 없음. USB 세 보드에 이 바이너리가 설치됐는지는 별도 확인합니다.

화면에는 요약만, 상세 로그·JSON은 실행마다 새 `build/test-results/` 폴더에 보존합니다.
[전체 검사 안내](../../README.md)

