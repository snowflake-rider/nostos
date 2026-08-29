#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
BUILD_DIR="${NOSTOS_TEST_BUILD_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/nostos-stm32-host.XXXXXX")}"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then GENERATOR=Ninja; fi
cmake -S "$ROOT_DIR/firmware/stm32/host-tests" -B "$BUILD_DIR" -G "$GENERATOR" -DENABLE_SANITIZERS=ON
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
