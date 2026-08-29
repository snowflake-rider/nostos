#!/bin/bash

set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$SCRIPT_DIR/../firmware/esp32"
IDF_EXPORT="${ESP_IDF_PATH:-${IDF_PATH:-$HOME/esp/esp-idf-v5.5.5}}/export.sh"

# The maintained ESP32 firmware is the only monitor target.
if (($# > 1)); then
    printf 'Usage: ESP_PORT=<port> %s\n' "$0" >&2
    exit 2
fi
case "${1:-}" in
    -h|--help)
        printf 'Usage: ESP_PORT=<port> %s\nMonitors firmware/esp32 (nostos_esp32).\n' "$0"
        exit 0
        ;;
    ''|8|layer-8) ;; # Keep existing Layer 8 invocations working.
    *)
        printf 'ERROR: learning layers were retired; this script monitors nostos_esp32 only.\n' >&2
        exit 2
        ;;
esac

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
printf 'Monitoring nostos_esp32 on %s\n' "$PORT"

cd "$PROJECT_DIR"
# shellcheck disable=SC1090
source "$IDF_EXPORT"
idf.py -p "$PORT" monitor
