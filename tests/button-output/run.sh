#!/usr/bin/env bash
# Builds/tests only. Never flashes, resets, opens serial ports, or drives hardware.
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
mode="${1:---host}"
if (($# > 1)); then printf 'Too many arguments\n' >&2; exit 2; fi

case "$mode" in
  --host|--targets|--all) ;;
  --help|-h)
    printf '%s\n' \
      'Usage: bash tests/button-output/run.sh [--host|--targets|--all]' \
      '--host (default): Debug/Release/ASan+UBSan host integration test.' \
      '--targets: Debug/Release STM32 diagnostic-mode compile/link only.' \
      '--all: both. No flash, reset, serial access, or physical output.'
    exit 0 ;;
  *) printf 'Unknown argument: %s\n' "$mode" >&2; exit 2 ;;
esac

BUILD_DIR="${NOSTOS_BUTTON_OUTPUT_BUILD_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/nostos-button-output.XXXXXX")}"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then
    GENERATOR=Ninja
fi
printf 'Button-output artifacts: %s\n' "$BUILD_DIR"

run_host() {
    command -v cmake >/dev/null
    command -v "${CC:-cc}" >/dev/null
    for variant in debug release sanitized; do
        local build_type=Debug sanitizers=OFF
        [[ "$variant" == release ]] && build_type=Release
        [[ "$variant" == sanitized ]] && sanitizers=ON
        cmake -S "$ROOT_DIR/tests/button-output" -B "$BUILD_DIR/host-$variant" \
            -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$build_type" \
            -DENABLE_SANITIZERS="$sanitizers"
        cmake --build "$BUILD_DIR/host-$variant" --parallel
        ctest --test-dir "$BUILD_DIR/host-$variant" --no-tests=error --output-on-failure
    done
}

add_installed_toolchains() {
    local base="${NOSTOS_TOOLCHAINS:-$HOME/.local/share/nostos-toolchains}"
    local additions=()
    if ! command -v arm-none-eabi-gcc >/dev/null 2>&1 && \
       [[ -d "$base/arm-15.3-extracted/Payload/bin" ]]; then
        additions+=("$base/arm-15.3-extracted/Payload/bin")
    fi
    if ! command -v ninja >/dev/null 2>&1 && [[ -d "$base/build-tools/bin" ]]; then
        additions+=("$base/build-tools/bin")
    fi
    if ((${#additions[@]})); then
        local joined
        joined="$(IFS=:; printf '%s' "${additions[*]}")"
        export PATH="$joined:$PATH"
    fi
}

run_targets() {
    add_installed_toolchains
    for tool in cmake ninja arm-none-eabi-gcc arm-none-eabi-size arm-none-eabi-nm; do
        command -v "$tool" >/dev/null || { printf 'Missing tool: %s\n' "$tool" >&2; exit 2; }
    done
    for variant in Debug Release; do
        local build="$BUILD_DIR/stm32-$variant"
        cmake --preset "$variant" -S "$ROOT_DIR/firmware/stm32" -B "$build" \
            -DBUTTON_OUTPUT_TEST=ON -DNOSTOS_PROTOCOL_V2=OFF -DCMAKE_C_FLAGS=-Werror
        cmake --build "$build" --parallel 8
        grep -q '^BUTTON_OUTPUT_TEST:BOOL=ON$' "$build/CMakeCache.txt"
        arm-none-eabi-size "$build/nostos_stm32.elf"
        arm-none-eabi-nm "$build/nostos_stm32.elf" | grep -q ' button_output_test_process$'
    done
}

[[ "$mode" == --host || "$mode" == --all ]] && run_host
[[ "$mode" == --targets || "$mode" == --all ]] && run_targets
printf '%s\n' 'BUTTON_OUTPUT_TEST=PASS; FLASH=NOT_PERFORMED; PHYSICAL_RGB_AND_SPEAKER_SOUND=NOT_TESTED'
