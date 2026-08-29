# 버튼 → RGB → 오디오 코덱

자동 검사:

```sh
bash tests/button-output/run.sh --all
```

`PB6 → STOP(0x13) → 흰색 RGB → 실제 stop_request MP3 배열 → VS1003B SDI SPI`를 포함해 5개 버튼 경로를 검사합니다. 호스트 검사는 HAL만 모의하고 실제 STM32 서비스·음원 배열·코덱 드라이버를 사용합니다. `--all`은 진단 모드 STM32 Debug/Release 컴파일·링크도 수행합니다.

진단 모드는 `BUTTON_OUTPUT_TEST=ON`일 때만 켜지고 기본값은 OFF입니다. 센서·원격 출력·버튼의 UART/Mesh 송신은 제외하고 이 STM32의 로컬 출력만 구동합니다.

| 입력 | 메시지 | RGB | 음원 |
| --- | --- | --- | --- |
| BTN1 PB5 | 감속 | 초록 | speed_down |
| BTN2 PB10 | 가속 | 빨강 | speed_up |
| BTN3 PA8 | 안전 | 파랑 | cheer_up |
| BTN4 PC7 / TEST PB6 | 정지 | 흰색 | stop_request |

`PASS`는 코드 경로와 빌드 성공입니다. 실제 LED·VS1003B·앰프·스피커 소리는 **NOT_TESTED**이며, 생성된 ELF를 Flash하지 않습니다. 실물 검사는 Flash 승인을 받은 뒤 각 버튼을 한 번 눌러 RGB가 2초 켜지고 해당 음성이 들리는지 사람이 확인해야 합니다.
