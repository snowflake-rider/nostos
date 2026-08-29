# 07. 버튼 → RGB → 오디오 코덱

```sh
bash tests/system/07-button-output/run.sh
```

PB6를 포함한 버튼 입력이 RGB, 실제 MP3 배열, VS1003B SPI 전송까지 이어지는지 자동 검사하고 진단 펌웨어를 Debug/Release로 빌드합니다.

Flash·보드 출력은 하지 않습니다. 자동 `PASS` 뒤에도 실제 LED와 스피커 소리는 별도 실물 확인이 필요합니다. 상세 매핑과 개별 실행은 [독립 테스트 README](../../button-output/README.md)를 보세요.
