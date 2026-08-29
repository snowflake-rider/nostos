# ESP32 큐 케이스 테스트

실물 보드 없이 **실제 `libs/protocol` C 코드**의 UART/Mesh 큐 동작을 반복 검사합니다.

```sh
bash tests/case-tests/run.sh --all
```

- [01-single](01-single/README.md): 입력 하나씩 용량·만료·출처·CRC 경계를 검사
- [02-complex](02-complex/README.md): FALL과 포화·폭주·중복·CLEAR·실패를 함께 검사

각 폴더의 `run.sh`로 한 단계만 실행할 수 있습니다. 모든 단계는 Debug, Release,
ASan/UBSan으로 실행됩니다. 실제 BLE packet, RF 손실률, Relay/다중 홉, UART 배선은 검사하지 않습니다.

현재 ESP32 기본 설정은 `CONFIG_NOSTOS_PROTOCOL_V2=n`이라 v1 큐를 사용합니다. 복합 테스트는
v1에서 일반 큐가 꽉 차면 FALL도 거절되는 현재 한계를 기록하고, v2의 긴급 예약 슬롯이 이를
막는지도 함께 검사합니다. v2 PASS만으로 실제 보드가 v2라고 판단하면 안 됩니다.
