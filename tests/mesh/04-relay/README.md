# 04. Relay 비교 — OFF → ON → OFF

목적: **D6(송신) → B6(중계) → 76(수신)** 배치에서 중계 효과를 비교합니다.
여기서 Relay는 Bluetooth Mesh 메시지 중계이며 실물 릴레이 스위치가 아닙니다.

준비:

- [02 송수신](../02-delivery/README.md)을 먼저 확인합니다. 03 반복 시험은 종료해 둡니다.
- D6↔76 직접 통신을 거리/차폐로 제한하고 B6를 양쪽에 닿는 위치에 둡니다.
- D6·76의 Relay는 OFF, 다른 중계기·우회 경로와 다른 송신은 제외합니다.
- 세 보드 USB 로그를 동시에 볼 수 있어야 합니다. 위치·출력·TTL·키 설정은 비교 내내 고정합니다.

```sh
bash tests/mesh/04-relay/run.sh --send
```

스크립트 질문에 따라 nRF Mesh에서 **B6만 OFF → ON(3 transmissions / 20 ms) → OFF**로 바꾸고,
매번 앱에서 재조회한 뒤 `yes`를 입력합니다. Console의 캐시된 `relay` 값으로 확인하지 않습니다.
각 구간 20회(약 100초), 마지막에는 자동 비교합니다. 스크립트가 Relay를 바꾸지는 않습니다.

결과: OFF 두 구간 수신 0건 + ON 전 회차 수신 등 비교 조건을 만족하면 `RELAY_EFFECT_OBSERVED`,
그 외에는 `INCONCLUSIVE`입니다. 정확한 패킷 홉 경로의 증명은 아닙니다.
결과는 한 실행 폴더의 `off-1/`, `on/`, `off-2/`와 `comparison.json`에 남습니다.

`Ctrl+C`/입력 취소/오류에서 멈추며 설정을 자동 복원하지 않습니다. 재실행은 새 폴더에서 처음부터 합니다.
[전체 순서](../README.md) · [상세 조건·한계](../../../docs/verification/mesh-repeat.md)
