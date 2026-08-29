#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ -n "${LAYER8_PYTHON:-}" ]]; then
    exec "$LAYER8_PYTHON" -u "$SCRIPT_DIR/tools/fast_check.py" "$@"
fi

# Reuse installed pyserial; no package install or ESP-IDF export needed.
for check_python in python3 "$HOME"/.espressif/python_env/idf*/bin/python; do
    if "$check_python" -c 'import serial' >/dev/null 2>&1; then
        exec "$check_python" -u "$SCRIPT_DIR/tools/fast_check.py" "$@"
    fi
done
printf '%s\n' 'pyserial 환경이 없습니다. LAYER8_PYTHON=/path/to/python bash fast-check.sh' >&2
exit 2
