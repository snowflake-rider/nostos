#!/usr/bin/env bash
# Host-only regression tests. Does not open serial ports or flash boards.
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
if [[ "${1:-}" == --help ]]; then
    printf 'Usage: bash tools/test-host.sh\nRuns C and Python host tests without hardware.\n'
    exit 0
fi
if (($#)); then printf 'Unexpected argument: %s\n' "$1" >&2; exit 2; fi
BUILD_DIR="${NOSTOS_TEST_BUILD_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/nostos-host.XXXXXX")}"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then GENERATOR=Ninja; fi
PYTHON="${NOSTOS_PYTHON:-python3}"
printf 'Host test artifacts: %s\n' "$BUILD_DIR"
for variant in debug release sanitized; do
    build_type=Debug
    sanitizers=OFF
    if [[ "$variant" == release ]]; then build_type=Release; fi
    if [[ "$variant" == sanitized ]]; then sanitizers=ON; fi
    for project in protocol message-protocol stm32 esp32 communication gps; do
        options=("-DENABLE_SANITIZERS=$sanitizers")
        case "$project" in
            protocol) source_dir="$ROOT_DIR/libs/protocol" ;;
            message-protocol) source_dir="$ROOT_DIR/tests/message-protocol" ;;
            stm32) source_dir="$ROOT_DIR/firmware/stm32/host-tests" ;;
            esp32) source_dir="$ROOT_DIR/firmware/esp32/host-tests" ;;
            communication)
                source_dir="$ROOT_DIR/experiments/communication-module"
                options=("-DCOMM_ENABLE_SANITIZERS=$sanitizers") ;;
            gps)
                source_dir="$ROOT_DIR/experiments/examples/esp32s3/gps-mesh-node/host-tests"
                # This existing suite always enables ASan/UBSan. Keep the array
                # nonempty for macOS Bash 3.2 with nounset enabled.
                options=(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON) ;;
        esac
        build_dir="$BUILD_DIR/$project-$variant"
        cmake -S "$source_dir" -B "$build_dir" -G "$GENERATOR" \
            -DCMAKE_BUILD_TYPE="$build_type" "${options[@]}"
        cmake --build "$build_dir" --parallel
        ctest --test-dir "$build_dir" --output-on-failure
    done
done
"$PYTHON" -m unittest discover -s "$ROOT_DIR/firmware/esp32/host-tests" -p 'test_*.py' -v
"$PYTHON" -m unittest discover -s "$ROOT_DIR/tests/integration" -p 'test_*.py' -v
"$PYTHON" -m unittest discover -s "$ROOT_DIR/tools/tests" -p 'test_*.py' -v
"$PYTHON" "$ROOT_DIR/tools/check_repository.py"
printf '%s\n' 'HOST_TESTS=PASS; HARDWARE_UART_AND_MESH=NOT_TESTED'
