#!/usr/bin/env bash
set -euo pipefail

V1_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
target="${1:-all}"

build_stm32() {
    command -v arm-none-eabi-gcc >/dev/null
    (
        cd "$V1_DIR/stm32"
        cmake --preset Release \
            -DNOSTOS_PROTOCOL_V2=OFF -DBUTTON_OUTPUT_TEST=OFF \
            -DSSD1306_DISPLAY=ON -DDHT11_SENSOR=ON
        cmake --build --preset Release --parallel
        arm-none-eabi-objcopy -O binary \
            build/Release/nostos_stm32.elf \
            build/Release/nostos_stm32.bin
    )
}

build_esp32() {
    command -v idf.py >/dev/null
    idf.py --version
    (cd "$V1_DIR/esp32" && idf.py build)
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
