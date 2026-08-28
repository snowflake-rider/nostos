#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
LOG_DIR="$SCRIPT_DIR/logs"
RUN_TIMESTAMP="$(TZ=Asia/Seoul date '+%Y%m%dT%H%M%S-KST')"
LOG_FILE="$LOG_DIR/esp32s3-layer-7-standard-mesh-triplet-$RUN_TIMESTAMP.log"
DEFAULT_IDF_DIR="/Users/kafka/esp/esp-idf-v5.5.5"
PORT_A=""
PORT_B=""
PORT_C=""
ERASE_FLASH="no"
RESULT="FAIL"
CURRENT_STAGE="initialization"

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
stage() { CURRENT_STAGE="$1"; printf '\nSTAGE=%s\n' "$CURRENT_STAGE"; }
usage() {
  printf '%s\n' \
    'Usage: ./bootload-triplet.sh [--port-a DEVICE --port-b DEVICE --port-c DEVICE] [--erase]' \
    'With no port arguments, exactly three supported serial ports are required.'
}
finish() {
  local exit_code=$?
  trap - EXIT
  if [[ "$RESULT" == "PASS" && $exit_code -eq 0 ]]; then
    printf '\nRESULT=PASS\nTRIPLET_BUILD_FLASH_BOOT=PASS\n'
    printf 'IPHONE_DISCOVERY=NOT_VERIFIED\nPROVISIONING=NOT_VERIFIED\n'
    printf 'CONFIGURATION=NOT_VERIFIED\nGROUP_ONOFF=NOT_VERIFIED\n'
    printf 'TRIPLET_RELAY=NOT_VERIFIED\n'
  else
    ((exit_code != 0)) || exit_code=1
    printf '\nRESULT=FAIL\nSTAGE=%s\nEXIT_CODE=%d\n' "$CURRENT_STAGE" "$exit_code"
  fi
  printf 'PORT_A=%s\nPORT_B=%s\nPORT_C=%s\nLOG_FILE=%s\n' \
    "${PORT_A:-not-selected}" "${PORT_B:-not-selected}" \
    "${PORT_C:-not-selected}" "$LOG_FILE"
  exit "$exit_code"
}

while (($# > 0)); do
  case "$1" in
  --port-a) (($# >= 2)) || fail '--port-a requires a value.'; PORT_A="$2"; shift 2 ;;
  --port-b) (($# >= 2)) || fail '--port-b requires a value.'; PORT_B="$2"; shift 2 ;;
  --port-c) (($# >= 2)) || fail '--port-c requires a value.'; PORT_C="$2"; shift 2 ;;
  --erase) ERASE_FLASH="yes"; shift ;;
  -h | --help) usage; exit 0 ;;
  *) fail "Unknown argument: $1" ;;
  esac
done

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1
trap finish EXIT

stage "toolchain_check"
if [[ -n "${ESP_IDF_PATH:-}" ]]; then
  IDF_INSTALL_DIR="$ESP_IDF_PATH"
elif [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
  IDF_INSTALL_DIR="$IDF_PATH"
else
  IDF_INSTALL_DIR="$DEFAULT_IDF_DIR"
fi
[[ -f "$IDF_INSTALL_DIR/export.sh" ]] || fail 'ESP-IDF export.sh was not found.'
# shellcheck disable=SC1091
source "$IDF_INSTALL_DIR/export.sh"

stage "triplet_detection"
ports=()
while IFS= read -r port; do ports+=("$port"); done < <(
  find /dev -maxdepth 1 \
    \( -name 'cu.usbmodem*' -o -name 'cu.usbserial*' \
    -o -name 'cu.SLAB_USBtoUART*' -o -name 'cu.wchusbserial*' \) \
    -print 2>/dev/null | sort
)
if [[ -z "$PORT_A" && -z "$PORT_B" && -z "$PORT_C" ]]; then
  ((${#ports[@]} == 3)) || fail 'Auto-selection requires exactly three serial ports.'
  PORT_A="${ports[0]}"; PORT_B="${ports[1]}"; PORT_C="${ports[2]}"
elif [[ -z "$PORT_A" || -z "$PORT_B" || -z "$PORT_C" ]]; then
  fail 'Specify all of --port-a, --port-b, and --port-c.'
fi
[[ "$PORT_A" != "$PORT_B" && "$PORT_A" != "$PORT_C" && "$PORT_B" != "$PORT_C" ]] || \
  fail 'All three ports must differ.'
for port in "$PORT_A" "$PORT_B" "$PORT_C"; do
  [[ -c "$port" ]] || fail "Invalid serial port: $port"
done
printf 'PORT_A=%s\nPORT_B=%s\nPORT_C=%s\n' "$PORT_A" "$PORT_B" "$PORT_C"

stage "triplet_profile"
for port in "$PORT_A" "$PORT_B" "$PORT_C"; do
  profile="$(esptool.py --chip esp32s3 --port "$port" --baud 115200 \
    --before default_reset --after hard_reset --no-stub chip_id)"
  printf '%s\n' "$profile"
  grep -q 'ESP32-S3' <<<"$profile" || fail "Non-ESP32-S3 device on $port"
done

stage "build_once"
cd "$SCRIPT_DIR"
if [[ ! -f sdkconfig ]] || ! grep -qx 'CONFIG_IDF_TARGET="esp32s3"' sdkconfig; then
  idf.py set-target esp32s3
fi
idf.py build
[[ -f build/esp32s3_layer_7.bin ]] || fail 'Layer 7 application binary is missing.'

for label in A B C; do
  case "$label" in A) port="$PORT_A" ;; B) port="$PORT_B" ;; C) port="$PORT_C" ;; esac
  if [[ "$ERASE_FLASH" == "yes" ]]; then
    stage "erase_board_$label"
    esptool.py --chip esp32s3 --port "$port" erase_flash
  fi
  stage "flash_board_$label"
  idf.py -p "$port" flash
done

stage "triplet_runtime_identity"
python - "$PORT_A" "$PORT_B" "$PORT_C" <<'PY'
import re
import sys
import time

import serial

ports = dict(zip("ABC", sys.argv[1:4]))
devices = {label: serial.Serial(port, 115200, timeout=0.1)
           for label, port in ports.items()}
identities = {}
ready = set()
deadline = time.monotonic() + 35
pattern = re.compile(r"\[LAYER-7\] NODE_IDENTITY node=([0-9A-F]{2})")

try:
    for device in devices.values():
        device.dtr = False
        device.rts = True
    time.sleep(0.1)
    for device in devices.values():
        device.rts = False

    while time.monotonic() < deadline:
        for label, device in devices.items():
            raw = device.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            print(f"[{label} {ports[label]}] {line}", flush=True)
            match = pattern.search(line)
            if match:
                identities[label] = match.group(1)
            if "[LAYER-7] NODE_ACTIVE" in line:
                ready.add(label)
        if len(identities) == 3 and len(set(identities.values())) == 3 and len(ready) == 3:
            print("TRIPLET_DISTINCT_IDENTITIES=PASS", flush=True)
            raise SystemExit(0)
finally:
    for device in devices.values():
        device.close()

print("TRIPLET_DISTINCT_IDENTITIES=FAIL", file=sys.stderr, flush=True)
raise SystemExit(1)
PY

RESULT="PASS"
CURRENT_STAGE="complete"
