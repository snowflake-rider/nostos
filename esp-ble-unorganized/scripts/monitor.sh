#!/bin/bash

set -e

LAYERS_DIR=/Users/kafka/Documents/Notion/esp-ble/layers
IDF_EXPORT=/Users/kafka/esp/esp-idf-v5.5.5/export.sh
DEFAULT_LAYER=layer-2

# Pick the project to monitor:  ./monitor.sh 3   or   ./monitor.sh layer-3
# No arg -> $DEFAULT_LAYER
LAYER="${1:-$DEFAULT_LAYER}"
[[ "$LAYER" =~ ^[0-9]+$ ]] && LAYER="layer-$LAYER"
PROJECT_DIR="$LAYERS_DIR/$LAYER"

if [[ ! -f "$PROJECT_DIR/CMakeLists.txt" ]]; then
    printf 'ERROR: no ESP-IDF project at %s\n' "$PROJECT_DIR" >&2
    exit 1
fi

# Find the ESP32 serial port dynamically. The usbmodem id changes between
# plug-ins, so scan the common macOS USB-serial names instead of hardcoding.
# Override with:  ESP_PORT=/dev/cu.xxx ./monitor.sh
detect_port() {
    if [[ -n "$ESP_PORT" ]]; then
        printf '%s\n' "$ESP_PORT"
        return
    fi

    local ports=()
    while IFS= read -r p; do
        ports+=("$p")
    done < <(
        find /dev -maxdepth 1 \
            \( -name 'cu.usbmodem*' -o -name 'cu.usbserial*' \
            -o -name 'cu.SLAB_USBtoUART*' -o -name 'cu.wchusbserial*' \) \
            -print 2>/dev/null | sort
    )

    if ((${#ports[@]} == 0)); then
        printf 'ERROR: no ESP32 USB serial port found. Is the board plugged in?\n' >&2
        exit 1
    fi
    if ((${#ports[@]} > 1)); then
        printf 'Multiple USB serial ports found:\n' >&2
        printf '  %s\n' "${ports[@]}" >&2
        printf 'Pick one with:  ESP_PORT=<port> %s\n' "$0" >&2
        exit 1
    fi
    printf '%s\n' "${ports[0]}"
}

PORT=$(detect_port)
printf 'Monitoring %s on %s\n' "$LAYER" "$PORT"

cd "$PROJECT_DIR"
# shellcheck disable=SC1090
source "$IDF_EXPORT"
idf.py -p "$PORT" monitor
