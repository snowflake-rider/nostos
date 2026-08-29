# 02 — 복합 케이스

```sh
bash tests/case-tests/02-complex/run.sh
```

5개 검사: 포화+FALL, 긴급 폭주+공정성, FALL/CLEAR+중복/역순,
UART→Mesh→UART+재송신 방지, wraparound+준비 상실+비정상 입력. 실제 RF는 검사하지 않습니다.

첫 검사의 PASS에는 `v1: FALL 거절 한계 재현`과 `v2: FALL 예약 슬롯 보호`가 모두 포함됩니다.
현재 기본 ESP32 펌웨어는 v1이므로 PASS를 현재 펌웨어의 긴급 보호 활성화로 해석하지 않습니다.
