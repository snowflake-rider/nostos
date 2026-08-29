#!/usr/bin/env bash
set -euo pipefail
export MESH_REPEAT_HUMAN=1

if [[ $# -gt 0 ]]; then
  printf '%s\n' '사용법: bash run.sh  (읽기 전용 준비 확인; 송신 없음)'
  [[ $# -eq 1 && "$1" == '--help' ]] && exit 0
  exit 2
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
exec bash "$ROOT_DIR/tools/hardware/run_mesh_repeat.sh" check --source D6 --peers 76 B6
