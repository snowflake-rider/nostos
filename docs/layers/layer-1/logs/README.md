> 이관 원문: `layers/layer-1/logs/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 1 Logs

`bootload.sh`가 이 디렉터리에 다음 이름으로 실행 기록을 만든다.

```text
esp32s3-layer-1-ble-advertising-YYYYMMDDTHHMMSS-KST.log
```

`RESULT=PASS`는 source/build/Flash/boot와 ESP-IDF Advertising start callback을 serial로 확인했다는 뜻이다. 스마트폰이 실제 radio packet을 받은 증거는 nRF Connect scan으로 별도 확인한다.
