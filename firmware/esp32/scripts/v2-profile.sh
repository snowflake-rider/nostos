#!/usr/bin/env bash
# Build or app-flash one v2 app image. Never erases or writes the NVS partition.
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
IDF_ROOT="${ESP_IDF_PATH:-${IDF_PATH:-$HOME/.local/share/nostos-toolchains/esp-idf-v5.5.5}}"
if [[ ! -f "$IDF_ROOT/export.sh" ]]; then
    printf 'ESP-IDF v5.5.5 not found: set ESP_IDF_PATH\n' >&2
    exit 78
fi
# ESP-IDF export.sh is not nounset-safe. Environment changes stay in this child shell.
set +u
source "$IDF_ROOT/export.sh" >/dev/null
set -u
exec python3 -B "$PROJECT_DIR/scripts/v2_profile.py" "$@"
