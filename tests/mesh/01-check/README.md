# 01. 준비 확인 — 송신 없음

목적: D6·76·B6의 연결과 시험 준비 상태를 확인합니다. 설정을 바꾸지 않습니다.

현재 설정만 먼저 보고 싶다면 `bash scripts/esp32-scan` — [빠른 조회 도구](../../../scripts/README.md).

1. [Mesh Console](../../../apps/mesh-console/README.md)을 실행하고 세 보드를 연결합니다. 웹 화면은 열어 둡니다.
2. nRF Mesh에서 기존 C001 설정은 유지한 채, 시험용 같은 AppKey로 아래를 확인합니다.
   - D6 **Generic OnOff Client**: Bind, Publication **C000**, TTL **7**, Period **0**.
   - 76·B6 **Generic OnOff Server**: Bind, Subscription **C000**.
3. 저장소 루트의 새 터미널에서 실행합니다.

```sh
bash tests/mesh/01-check/run.sh
```

다음 단계: `READY`가 나오고 수신 Server 설정도 직접 확인했다면 [02 송수신](../02-delivery/README.md)으로 갑니다.
`BLOCKED`/`STOP`이면 여기서 멈춥니다. `C000_SOURCE_NOT_READY`는 D6의 Client 설정을 먼저 확인하세요.

주의: 스크립트는 수신 Server의 Bind/Subscription까지 자동 확인하지 못합니다.
준비 완료는 무선 수신 성공이 아닙니다. [전체 순서](../README.md)
