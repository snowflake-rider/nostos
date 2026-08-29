#!/usr/bin/env bash
# One safe entrypoint; no package install, flash, reset or RF transmission.
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
exec "${NOSTOS_PYTHON:-python3}" -B -u "$ROOT_DIR/tools/testing/run.py" "$@"
