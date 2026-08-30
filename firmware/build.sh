#!/usr/bin/env bash
set -euo pipefail

FIRMWARE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
target="${1:-all}"

build_stm32() {
    command -v arm-none-eabi-gcc >/dev/null
    (
        cd "$FIRMWARE_DIR/stm32"
        cmake_args=(
            --preset Release
            -DNOSTOS_PROTOCOL_V2=OFF
            -DBUTTON_OUTPUT_TEST=OFF
            -DSSD1306_DISPLAY=ON
            -DMPU6050_SENSOR=OFF
            -DDHT11_SENSOR=OFF
        )
        if ! command -v ninja >/dev/null 2>&1; then
            cmake_args+=(-G "Unix Makefiles")
        fi
        cmake "${cmake_args[@]}"
        cmake --build --preset Release --parallel
        arm-none-eabi-objcopy -O binary \
            build/Release/nostos_stm32.elf \
            build/Release/nostos_stm32.bin
    )
}

build_esp32() {
    command -v idf.py >/dev/null
    idf_version="$(idf.py --version)"
    printf '%s\n' "$idf_version"
    case "$idf_version" in
        *v5.5.5*|*" 5.5.5"*) ;;
        *)
            printf 'error: ESP-IDF v5.5.5 is required; found: %s\n' "$idf_version" >&2
            return 1
            ;;
    esac
    (cd "$FIRMWARE_DIR/esp32" && idf.py build)
}

case "$target" in
    stm32) build_stm32 ;;
    esp32) build_esp32 ;;
    all)
        build_stm32
        build_esp32
        ;;
    *)
        printf 'usage: %s [all|stm32|esp32]\n' "$0" >&2
        exit 2
        ;;
esac
