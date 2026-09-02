#!/usr/bin/env bash
set -euo pipefail

FIRMWARE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
MODE="${1:-full}"
case "$MODE" in
    --fast)
        fast=1
        protocol_variant=fast
        protocol_sanitizers=OFF
        ;;
    full)
        fast=0
        protocol_variant=sanitized
        protocol_sanitizers=ON
        ;;
    *)
        printf 'usage: %s [--fast]\n' "$0" >&2
        exit 2
        ;;
esac

if [[ "$fast" -eq 1 ]]; then
    bash "$FIRMWARE_DIR/stm32/tools/test-host.sh" --fast
    bash "$FIRMWARE_DIR/esp32/test-host.sh" --fast
else
    bash "$FIRMWARE_DIR/stm32/tools/test-host.sh"
    bash "$FIRMWARE_DIR/esp32/test-host.sh"
fi

protocol_build="${NOSTOS_PROTOCOL_BUILD_DIR:-${NOSTOS_V1_PROTOCOL_BUILD_DIR:-$FIRMWARE_DIR/out/host-tests/protocol/$protocol_variant}}"
cmake -S "$FIRMWARE_DIR/protocol" -B "$protocol_build" -DENABLE_SANITIZERS="$protocol_sanitizers"
cmake --build "$protocol_build" --parallel
ctest --test-dir "$protocol_build" --output-on-failure

if [[ "$fast" -eq 0 ]]; then
    python3 "$FIRMWARE_DIR/tools/test_release.py"
fi

printf '%s\n' 'NOSTOS_HOST_TESTS=PASS; PHYSICAL_HARDWARE=NOT_TESTED_BY_THIS_SCRIPT'
