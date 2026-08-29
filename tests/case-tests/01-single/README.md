# 01 — 단일 케이스

```sh
bash tests/case-tests/01-single/run.sh
```

5개 검사: v1 입력/만료, v1 FIFO 용량, v2 긴급 예약/복사, v2 출처/만료,
UART 분할 frame/CRC 오류. 실제 무선 송신은 하지 않습니다.
