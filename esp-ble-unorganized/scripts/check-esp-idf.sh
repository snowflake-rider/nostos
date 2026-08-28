#!/usr/bin/env bash

set -e

IDF_INSTALL_DIR="${ESP_IDF_PATH:-/Users/kafka/esp/esp-idf-v5.5.5}"

if [[ ! -f "$IDF_INSTALL_DIR/export.sh" ]]; then
  printf 'ERROR: ESP-IDF export.sh not found: %s\n' "$IDF_INSTALL_DIR/export.sh" >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$IDF_INSTALL_DIR/export.sh"

command -v idf.py
idf.py --version
command -v esptool.py
esptool.py version

printf 'CHECK=PASS\n'
