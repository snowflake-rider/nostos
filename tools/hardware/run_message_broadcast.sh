#!/usr/bin/env bash
set -euo pipefail
TESTING_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ -n "${MESSAGE_TEST_PYTHON:-}" ]]; then
    exec "$MESSAGE_TEST_PYTHON" -u "$TESTING_DIR/message_broadcast.py" "$@"
fi
for test_python in python3 "$HOME"/.espressif/python_env/idf*/bin/python; do
    if "$test_python" -c 'import serial' >/dev/null 2>&1; then
        exec "$test_python" -u "$TESTING_DIR/message_broadcast.py" "$@"
    fi
done
printf '%s\n' 'pyserial 환경이 필요합니다. MESSAGE_TEST_PYTHON=/path/to/python 을 지정하세요.' >&2
exit 2
