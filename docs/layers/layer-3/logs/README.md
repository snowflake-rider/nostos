> 이관 원문: `layers/layer-3/logs/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 3 Logs

`bootload.sh`가 이 디렉터리에 다음 이름으로 실행 기록을 만든다.

```text
esp32s3-layer-3-active-scanner-YYYYMMDDTHHMMSS-KST.log
```

`RESULT=PASS`는 source/build/Flash/boot, Active Scanner 시작, 그리고
Board A의 `ESP32-LAYER-2` 이름과 Service UUID를 Board B가 실제 BLE
radio로 수신했다는 뜻이다.
