#!/usr/bin/env bash
set -euo pipefail
redpill_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
for redpill_mode in Debug Release Sanitized; do
    redpill_type="$redpill_mode"
    redpill_sanitizer=OFF
    if [[ "$redpill_mode" == Sanitized ]]; then
        redpill_type=Debug
        redpill_sanitizer=ON
    fi
    cmake -S "$redpill_root" -B "$redpill_root/build/$redpill_mode" \
        -DCMAKE_BUILD_TYPE="$redpill_type" -DREDPILL_SANITIZE="$redpill_sanitizer"
    cmake --build "$redpill_root/build/$redpill_mode" --parallel 2
    ctest --test-dir "$redpill_root/build/$redpill_mode" --output-on-failure
done
python3 "$redpill_root/tests/verify.py" "$redpill_root/build/Debug/redpill_demo"
