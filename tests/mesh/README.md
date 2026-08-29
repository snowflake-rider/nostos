# ESP32 Mesh 시험 — 여기서 시작

코드·핀·컴파일부터 확인하려면 [통합 검사](../README.md): `bash tests/run.sh`.

**한 단계씩 실행하세요. 처음에는 01 폴더만 보면 됩니다.**

| 순서 | 폴더 | 하는 일 |
| --- | --- | --- |
| 01 | [준비 확인](01-check/README.md) | 연결·설정 조회만, 송신 없음 |
| 02 | [짧은 송수신](02-delivery/README.md) | D6 → 76·B6, 6회 / 약 30초 |
| 03 | [계속 반복](03-repeat/README.md) | 같은 시험을 Ctrl+C 또는 로그 한도까지 |
| 04 | [Relay 비교](04-relay/README.md) | D6 → B6(중계) → 76의 효과 비교 |

각 폴더에는 **README.md + run.sh**가 있습니다. 모든 명령은 저장소 루트 기준입니다.
해당 폴더로 이동했다면 `bash run.sh`에 같은 옵션을 붙여도 됩니다.

```sh
bash tests/mesh/01-check/run.sh
```

공통: Mesh Console은 `127.0.0.1:8787`에서 열어 두고, 시험 중 다른 ON/OFF 조작은 멈춥니다.
02–04는 `--send`가 있어야 송신합니다. 이 시험은 **C000 OnOff 수신 로그**를 확인하며,
실물 릴레이 접점·C001 이벤트·STM32 출력·정확한 무선 경로를 검증하지 않습니다.

결과는 `build/hardware-results/`의 새 폴더에 저장됩니다. `summary.json`은 요약,
`trials.csv`는 회차별 결과입니다. 종료 코드 0만으로 무선 성공을 판단하지 않습니다.
중단 시 ON/OFF·Relay 상태를 자동 복원하지 않습니다.

단계 스크립트는 짧은 한국어 결과를 표시합니다. 준비 `READY`, 수신 로그 일치 `OBSERVED`,
송수신 누락 `FAIL`, 불완전/모호 `INCONCLUSIVE`, 사용자 중단 `CANCELLED`로 구분합니다.
02–03 종료 코드: 0=완료한 로그 관찰, 1=수신 누락, 2=차단/불완전, 130=중단.
04의 Relay-OFF 수신 누락은 비교용 증거이므로 해당 구간 자체를 실패로 종료하지 않습니다.
고급 `tools/hardware/run_mesh_repeat.sh`의 기존 JSON/종료 동작은 유지합니다.

[고급 옵션·판정 한계](../../docs/verification/mesh-repeat.md) · [개발자용 모의 검사](../integration/README.md)
