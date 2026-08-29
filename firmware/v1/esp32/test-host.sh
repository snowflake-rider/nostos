#!/usr/bin/env bash
set -euo pipefail

LAYER_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then GENERATOR=Ninja; fi
for variant in debug release sanitized; do
    build_type=Debug
    sanitizers=OFF
    if [[ "$variant" == release ]]; then build_type=Release; fi
    if [[ "$variant" == sanitized ]]; then sanitizers=ON; fi
    build_dir="$LAYER_DIR/host-tests/build/$variant"
    cmake -S "$LAYER_DIR/host-tests" -B "$build_dir" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$build_type" -DENABLE_SANITIZERS="$sanitizers"
    cmake --build "$build_dir"
    ctest --test-dir "$build_dir" --output-on-failure
done
