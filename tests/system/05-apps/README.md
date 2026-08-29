# 05. Console·TUI

```sh
bash tests/system/05-apps/run.sh
```

폴더 안에서는 `bash run.sh`. `--json`은 최종 JSON만 출력합니다.

기존 Mesh Console Python/React 테스트·웹 빌드, TUI 타입 검사·테스트를 실행합니다. 실제 USB나 송신은 사용하지 않습니다.

앱의 기존 `.venv`·`node_modules`를 사용하며 없으면 BLOCKED입니다. 자동 설치하지 않습니다. [Console 준비](../../../apps/mesh-console/README.md) · [TUI 준비](../../../apps/esp32-tui/README.md)

화면에는 요약만, 상세 로그·JSON은 실행마다 새 `build/test-results/` 폴더에 보존합니다.
[전체 검사 안내](../../README.md)

