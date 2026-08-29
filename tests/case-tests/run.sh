#!/usr/bin/env bash
# Host-only queue scenarios. No serial, flash, provisioning or RF transmission.
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
selection="${1:---all}"
case "$selection" in
  --all|--single|--complex) ;;
  --help|-h)
    printf '%s\n' \
      'Usage: bash tests/case-tests/run.sh [--all|--single|--complex]' \
      'Runs Debug, Release and ASan/UBSan against the real libs/protocol C code.' \
      'No serial, flash, provisioning or real Bluetooth/RF activity.'
    exit 0 ;;
  *) printf 'Unknown argument: %s\n' "$selection" >&2; exit 2 ;;
esac
if (($# > 1)); then
  printf 'Too many arguments\n' >&2
  exit 2
fi

command -v cmake >/dev/null
command -v "${CC:-cc}" >/dev/null
if [[ -n "${NOSTOS_CASE_TEST_BUILD_DIR:-}" ]]; then
  BUILD_DIR="$NOSTOS_CASE_TEST_BUILD_DIR"
  mkdir -p "$BUILD_DIR"
else
  BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/nostos-case-tests.XXXXXX")"
fi
printf 'Artifacts: %s\n' "$BUILD_DIR"
trap 'result=$?; if ((result)); then printf "CASE_TESTS=FAIL exit=%s artifacts=%s\n" "$result" "$BUILD_DIR" >&2; fi' EXIT

pattern='case_(single|complex)'
[[ "$selection" == --single ]] && pattern='case_single'
[[ "$selection" == --complex ]] && pattern='case_complex'

for variant in debug release sanitized; do
  build_type=Debug
  sanitizers=OFF
  [[ "$variant" == release ]] && build_type=Release
  [[ "$variant" == sanitized ]] && sanitizers=ON
  cmake -S "$ROOT_DIR/tests/case-tests" -B "$BUILD_DIR/$variant" \
    -DCMAKE_BUILD_TYPE="$build_type" -DENABLE_SANITIZERS="$sanitizers" \
    >"$BUILD_DIR/$variant-configure.log" 2>&1 || {
      sed -n '1,240p' "$BUILD_DIR/$variant-configure.log"
      exit 1
    }
  cmake --build "$BUILD_DIR/$variant" --parallel \
    >"$BUILD_DIR/$variant-build.log" 2>&1 || {
      sed -n '1,240p' "$BUILD_DIR/$variant-build.log"
      exit 1
    }
  ctest --test-dir "$BUILD_DIR/$variant" --no-tests=error \
    --output-on-failure -V -R "^(${pattern})$" | tee "$BUILD_DIR/$variant-tests.log"
done
printf '%s\n' "CASE_TESTS=PASS selection=${selection#--}; V1_FALL_RESERVE=NO; V2_FALL_RESERVE=YES; REAL_BLE_RF=NOT_TESTED; HARDWARE=NOT_TOUCHED"
