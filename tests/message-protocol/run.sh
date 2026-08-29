#!/usr/bin/env bash
# No serial access, no provisioning/flash, no software installation.
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
case "${1:-}" in
  --help|-h)
    printf 'Usage: bash tests/message-protocol/run.sh [--all|--targets]\nDefault: v2 codec/UART/state/bridge/mock relay, Debug+Release+ASan/UBSan.\n--all: repository host regressions plus complete v2 STM32/ESP32 builds.\n--targets: only complete v2 firmware builds using installed toolchains.\nNo installation, flash, serial access or real BLE/RF test.\n'
    exit 0 ;;
  ''|--all|--targets) ;;
  *) printf 'Unknown argument: %s\n' "$1" >&2; exit 2 ;;
esac
if (($#>1)); then printf 'Too many arguments\n' >&2; exit 2; fi
if [[ "${1:-}" == --all ]]; then
    bash "$ROOT_DIR/tools/test-host.sh"
    "${NOSTOS_PYTHON:-python3}" "$ROOT_DIR/tests/message-protocol/build_targets.py"
    printf '%s\n' 'MESSAGE_PROTOCOL=PASS; MOCK_UART_AND_RELAY=PASS; REAL_BLE_RF=NOT_TESTED; HARDWARE_OUTPUTS=NOT_TESTED'
    exit 0
fi
if [[ "${1:-}" == --targets ]]; then
    exec "${NOSTOS_PYTHON:-python3}" "$ROOT_DIR/tests/message-protocol/build_targets.py"
fi
command -v cmake >/dev/null
command -v "${CC:-cc}" >/dev/null
command -v "${NOSTOS_PYTHON:-python3}" >/dev/null
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/nostos-message-protocol.XXXXXX")"
printf 'Artifacts: %s\n' "$BUILD_DIR"
trap 'result=$?; if ((result)); then printf "MESSAGE_PROTOCOL=FAIL exit=%s artifacts=%s\n" "$result" "$BUILD_DIR" >&2; fi' EXIT
for variant in debug release sanitized; do
    build_type=Debug; sanitizers=OFF
    if [[ "$variant" == release ]]; then build_type=Release; fi
    if [[ "$variant" == sanitized ]]; then sanitizers=ON; fi
    cmake -S "$ROOT_DIR/tests/message-protocol" -B "$BUILD_DIR/$variant" \
        -DCMAKE_BUILD_TYPE="$build_type" -DENABLE_SANITIZERS="$sanitizers" \
        -DPython3_EXECUTABLE="$(command -v "${NOSTOS_PYTHON:-python3}")" >"$BUILD_DIR/$variant-configure.log" 2>&1 || { cat "$BUILD_DIR/$variant-configure.log"; exit 1; }
    cmake --build "$BUILD_DIR/$variant" --parallel >"$BUILD_DIR/$variant-build.log" 2>&1 || { cat "$BUILD_DIR/$variant-build.log"; exit 1; }
    ctest --test-dir "$BUILD_DIR/$variant" --no-tests=error --output-on-failure -V | tee "$BUILD_DIR/$variant-tests.log"
done
printf '%s\n' 'MESSAGE_PROTOCOL=PASS; MOCK_UART_AND_RELAY=PASS; REAL_BLE_RF=NOT_TESTED; HARDWARE_OUTPUTS=NOT_TESTED'
