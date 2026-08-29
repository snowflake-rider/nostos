#!/usr/bin/env bash
set -euo pipefail
export MESH_REPEAT_HUMAN=1

if [[ $# -ne 1 || "${1:-}" != '--send' ]]; then
  printf '%s\n' '사용법: bash run.sh --send  (D6 → 76·B6, ON/OFF 6회, 약 30초)' \
    '먼저 01 준비 확인을 마치세요. --send 없이는 송신하지 않습니다.'
  [[ $# -eq 1 && "$1" == '--help' ]] && exit 0
  exit 2
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
printf '%s\n' '02 송수신: 6회 C000 수신 로그 확인. OBSERVED는 로그 일치이며 ACK/다중 홉 증거가 아닙니다.'
exec bash "$ROOT_DIR/tools/hardware/run_mesh_repeat.sh" run --send \
  --source D6 --peers 76 B6 --count 6 --interval 5 --window 3
