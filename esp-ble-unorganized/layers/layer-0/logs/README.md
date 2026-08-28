# Layer 0 Logs

`bootload.sh`가 이 디렉터리에 다음 이름으로 실행 기록을 만든다.

```text
esp32s3-layer-0-bootload-YYYYMMDDTHHMMSS-KST.log
```

로그에는 USB profile, chip/flash 정보, build, Flash, serial runtime checkpoint와 최종 `RESULT=PASS|FAIL`이 들어간다.
