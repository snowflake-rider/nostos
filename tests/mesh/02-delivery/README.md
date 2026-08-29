# 02. 짧은 송수신 — 약 30초

준비: [01 준비 확인](../01-check/README.md)을 마친 뒤 세 보드를 가까이 둡니다.
목적: D6가 ON/OFF를 번갈아 **6회**, 5초 간격으로 보내고 76·B6의 수신 로그를 확인합니다.

```sh
bash tests/mesh/02-delivery/run.sh --send
```

다음 단계: 6회 모두 `RECEIVE_MATCH_OBSERVED`이고 요약이 `matched=6`, `missing=0`,
`ambiguous=0`, `stop=COMPLETED`이면 [03 반복 시험](../03-repeat/README.md)으로 갑니다.

- `MISSING_RECEIVE`: 해당 회차의 수신 로그가 부족합니다. 연결·C000 수신 설정을 확인합니다.
- `AMBIGUOUS` / `STOP`: 다른 명령·중복·끊김 등의 원인을 확인한 뒤 다시 시작합니다.

결과 경로는 마지막 `SUMMARY`에 나옵니다. `Ctrl+C`로 중단할 수 있습니다.
종료 코드 0만으로 통과가 아니며, 중단 시 자동 OFF/상태 복원을 하지 않습니다.
가까운 보드의 수신은 Relay 중계 증명이 아닙니다. [전체 순서](../README.md)
