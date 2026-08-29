# 01. 핀 설정

```sh
bash tests/system/01-pins/run.sh
```

폴더 안에서는 `bash run.sh`. `--json`은 최종 JSON만 출력합니다.

STM32의 IOC·main.h·GPIO 초기화와 ESP32 UART/USB 설정을 대조합니다. 핀 중복, AF/모드/명시된 Pull, UART 115200/8N1 계약 불일치를 검사합니다.

PASS는 **현재 소스의 지원되는 초기화 형식**이 일치한다는 뜻입니다. 해석 불가능한 새 초기화는 BLOCKED입니다. 실제 배선·전압·신호·HAL 내부·system_stm32f4xx.c의 조건부 외부 메모리 경로는 검사하지 않습니다.

화면에는 요약만, 상세 로그·JSON은 실행마다 새 `build/test-results/` 폴더에 보존합니다.
[전체 검사 안내](../../README.md)

