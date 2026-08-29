#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

if ! command -v idf.py >/dev/null 2>&1; then
    if [[ -z "${IDF_PATH:-}" || ! -f "${IDF_PATH}/export.sh" ]]; then
        echo "ESP-IDF v5.5.5 environment is required (set IDF_PATH and source export.sh)." >&2
        exit 2
    fi
    # shellcheck disable=SC1090
    source "${IDF_PATH}/export.sh" >/dev/null
fi

version="$(idf.py --version)"
if [[ "$version" != *"v5.5.5"* && "$version" != *"5.5.5"* ]]; then
    echo "Expected ESP-IDF v5.5.5, got: $version" >&2
    exit 2
fi

idf.py -C "$PROJECT_DIR" build
