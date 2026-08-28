> 이관 원문: `layers/layer-2/logs/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 2 Logs

`bootload.sh`가 이 디렉터리에 다음 이름으로 실행 기록을 만든다.

```text
esp32s3-layer-2-gatt-server-YYYYMMDDTHHMMSS-KST.log
```

`RESULT=PASS`는 source/build/Flash/boot와 ESP-IDF GATT Service 및
connectable Advertising 초기화를 serial로 확인했다는 뜻이다.

휴대폰의 실제 Connect, RX Write, TX Read, TX Notify는 nRF Connect에서
별도로 확인하며, 확인 전에는 `PHONE_GATT_TEST=NOT_VERIFIED` 상태다.
