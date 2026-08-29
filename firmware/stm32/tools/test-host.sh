#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../host-tests" && pwd -P)"
BUILD_ROOT="${NOSTOS_TEST_BUILD_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/nostos-stm32-calibration.XXXXXX")}"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"

if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then
    GENERATOR=Ninja
fi

printf 'STM32 calibration host artifacts: %s\n' "$BUILD_ROOT"
for variant in debug release sanitized; do
    build_type=Debug
    sanitizers=OFF
    [[ "$variant" == release ]] && build_type=Release
    [[ "$variant" == sanitized ]] && sanitizers=ON

    cmake -S "$SOURCE_DIR" -B "$BUILD_ROOT/$variant" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DENABLE_SANITIZERS="$sanitizers"
    cmake --build "$BUILD_ROOT/$variant" --parallel
    ctest --test-dir "$BUILD_ROOT/$variant" --output-on-failure
done

printf '%s\n' 'STM32_CALIBRATION_HOST_TESTS=PASS; HARDWARE_AND_BICYCLE=NOT_TESTED'
