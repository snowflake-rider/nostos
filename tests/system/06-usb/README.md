# 06. USB 상태 조회

```sh
bash tests/system/06-usb/run.sh
```

폴더 안에서는 `bash run.sh`. `--json`은 최종 JSON만 출력합니다.

D6·76·B6를 기존 스캐너로 식별하고 새 STATUS를 조회합니다. 필요한 내부 서버는 자동 준비하고 다른 프로세스가 쓰는 포트는 빼앗지 않습니다. ON/OFF 송신·설정·Flash 없음.

정상 결과도 **READ**입니다. `usb.json`의 각 보드 준비 값과 조회 불가 필드를 확인하세요. Client 준비 0은 원인 확정이 아닙니다. 이미 앱에서 설정했다면 재설정 전에 캐시/내부 판정을 진단해야 합니다.

[조회 항목과 한계](../../../scripts/README.md) · [다음: C000 Mesh 시험](../../mesh/README.md)

화면에는 요약만, 상세 로그·JSON은 실행마다 새 `build/test-results/` 폴더에 보존합니다.
[전체 검사 안내](../../README.md)

