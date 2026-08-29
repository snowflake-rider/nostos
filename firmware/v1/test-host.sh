#!/usr/bin/env bash
set -euo pipefail

V1_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

bash "$V1_DIR/stm32/tools/test-host.sh"
bash "$V1_DIR/esp32/test-host.sh"

protocol_build="${NOSTOS_V1_PROTOCOL_BUILD_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/nostos-v1-protocol.XXXXXX")}"
cmake -S "$V1_DIR/protocol" -B "$protocol_build" -DENABLE_SANITIZERS=ON
cmake --build "$protocol_build" --parallel
ctest --test-dir "$protocol_build" --output-on-failure

printf '%s\n' 'NOSTOS_V1_HOST_TESTS=PASS; PHYSICAL_HARDWARE=NOT_TESTED_BY_THIS_SCRIPT'
