#!/usr/bin/env bash
set -euo pipefail

FIRMWARE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

bash "$FIRMWARE_DIR/stm32/tools/test-host.sh"
bash "$FIRMWARE_DIR/esp32/test-host.sh"

protocol_build="${NOSTOS_PROTOCOL_BUILD_DIR:-${NOSTOS_V1_PROTOCOL_BUILD_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/nostos-protocol.XXXXXX")}}"
cmake -S "$FIRMWARE_DIR/protocol" -B "$protocol_build" -DENABLE_SANITIZERS=ON
cmake --build "$protocol_build" --parallel
ctest --test-dir "$protocol_build" --output-on-failure

printf '%s\n' 'NOSTOS_HOST_TESTS=PASS; PHYSICAL_HARDWARE=NOT_TESTED_BY_THIS_SCRIPT'
