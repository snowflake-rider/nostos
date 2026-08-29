#!/usr/bin/env bash
# Called by the test runner, with an isolated output directory.
set -euo pipefail
target="$1"; source_dir="$2"; output_dir="$3"
if [[ "$target" == stm32-* ]]; then
    for tool in cmake ninja arm-none-eabi-gcc arm-none-eabi-size; do
        command -v "$tool" >/dev/null || { printf 'Missing tool: %s\n' "$tool"; exit 2; }
    done
    arm-none-eabi-gcc --version
    cd "$source_dir"
    variant="${target#stm32-}"
    cmake --preset "$variant" -B "$output_dir" -D CMAKE_C_FLAGS=-Werror
    cmake --build "$output_dir" --parallel 8
    arm-none-eabi-size "$output_dir/nostos_stm32.elf"
    exit
fi
if [[ "$target" != esp32 ]]; then printf 'Unknown build target\n'; exit 2; fi
if [[ ! -f "${ESP_IDF_PATH:-}/export.sh" ]]; then
    printf 'ESP_IDF_PATH must point to ESP-IDF v5.5.5\n'; exit 78
fi
# export.sh isn't nounset-safe. All changes are confined to this child process.
set +u
source "$ESP_IDF_PATH/export.sh" || exit 78
set -u
version="$(idf.py --version)" || exit 78
printf '%s\n' "$version"
[[ "$version" == 'ESP-IDF v5.5.5' ]] || { printf 'Expected ESP-IDF v5.5.5\n'; exit 78; }
cd "$source_dir"
idf.py -B "$output_dir" -D "SDKCONFIG=$source_dir/sdkconfig" -D IDF_TARGET=esp32s3 build
