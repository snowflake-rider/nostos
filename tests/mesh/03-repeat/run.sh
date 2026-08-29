#!/usr/bin/env bash
set -euo pipefail
export MESH_REPEAT_HUMAN=1

if [[ $# -ne 1 || "${1:-}" != '--send' ]]; then
  printf '%s\n' '사용법: bash run.sh --send  (D6 → 76·B6, Ctrl+C/로그 한도까지 반복)' \
    '먼저 02 송수신을 확인하세요. --send 없이는 송신하지 않습니다.'
  [[ $# -eq 1 && "$1" == '--help' ]] && exit 0
  exit 2
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
printf '%s\n' '03 반복: Ctrl+C로 종료합니다. 수신 누락은 기록 후 계속, 오류/로그 한도에서는 중단합니다.'
exec bash "$ROOT_DIR/tools/hardware/run_mesh_repeat.sh" run --send \
  --source D6 --peers 76 B6 --count 0 --interval 5 --window 3 --max-log-mb 50
