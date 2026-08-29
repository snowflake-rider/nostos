#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
# Reuse the console's pinned websockets runtime; never install packages implicitly.
if [[ -n "${MESH_REPEAT_PYTHON:-}" ]]; then
    exec "$MESH_REPEAT_PYTHON" -B -u "$ROOT_DIR/tools/hardware/mesh_repeat.py" "$@"
fi
if [[ -x "$ROOT_DIR/apps/mesh-console/.venv/bin/python" ]]; then
    exec "$ROOT_DIR/apps/mesh-console/.venv/bin/python" -B -u "$ROOT_DIR/tools/hardware/mesh_repeat.py" "$@"
fi
exec python3 -B -u "$ROOT_DIR/tools/hardware/mesh_repeat.py" "$@"
