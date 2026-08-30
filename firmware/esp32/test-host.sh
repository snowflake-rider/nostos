#!/usr/bin/env bash
set -euo pipefail

LAYER_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
FIRMWARE_DIR="$(cd -- "$LAYER_DIR/.." && pwd -P)"
BUILD_ROOT="${NOSTOS_ESP32_TEST_BUILD_DIR:-$FIRMWARE_DIR/out/host-tests/esp32}"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
MODE="${1:-full}"
case "$MODE" in
    --fast) variants=(fast) ;;
    full) variants=(debug release sanitized) ;;
    *) printf 'usage: %s [--fast]\n' "$0" >&2; exit 2 ;;
esac
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then GENERATOR=Ninja; fi
for variant in "${variants[@]}"; do
    build_type=Debug
    sanitizers=OFF
    if [[ "$variant" == release ]]; then build_type=Release; fi
    if [[ "$variant" == sanitized ]]; then sanitizers=ON; fi
    build_dir="$BUILD_ROOT/$variant"
    cmake -S "$LAYER_DIR/host-tests" -B "$build_dir" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$build_type" -DENABLE_SANITIZERS="$sanitizers"
    cmake --build "$build_dir"
    ctest --test-dir "$build_dir" --output-on-failure
done
