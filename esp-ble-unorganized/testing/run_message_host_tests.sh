#!/usr/bin/env bash
set -euo pipefail
TESTING_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT_DIR="$(cd -- "$TESTING_DIR/.." && pwd -P)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/esp-ble-message-host.XXXXXX")"
printf 'Host test artifacts: %s\n' "$BUILD_DIR"
python3 -m unittest discover -s "$TESTING_DIR/tests" -v
python3 -m unittest discover -s "$ROOT_DIR/layers/layer-8/host-tests" -p 'test_*.py' -v
for project in stm32 layer8; do
    source_dir="$ROOT_DIR/stm32-project/integration/stm32/host-tests"
    if [[ "$project" == layer8 ]]; then source_dir="$ROOT_DIR/layers/layer-8/host-tests"; fi
    cmake -S "$source_dir" -B "$BUILD_DIR/$project" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
    cmake --build "$BUILD_DIR/$project"
    ctest --test-dir "$BUILD_DIR/$project" --output-on-failure
done
printf '%s\n' 'HOST_TESTS=PASS; HARDWARE_UART_AND_MESH=NOT_TESTED'
