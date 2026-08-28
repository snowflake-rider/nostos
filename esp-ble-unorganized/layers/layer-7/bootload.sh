#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$SCRIPT_DIR"
LOG_DIR="$PROJECT_DIR/logs"
RUN_TIMESTAMP="$(TZ=Asia/Seoul date '+%Y%m%dT%H%M%S-KST')"
LOG_FILE="$LOG_DIR/esp32s3-layer-7-standard-mesh-$RUN_TIMESTAMP.log"
DEFAULT_IDF_DIR="/Users/kafka/esp/esp-idf-v5.5.5"
EXPECTED_TARGET="esp32s3"
EXPECTED_FLASH_SIZE="16MB"
MONITOR_BAUD="115200"
MONITOR_TIMEOUT_SECONDS="30"

CURRENT_STAGE="initialization"
RESULT="FAIL"
PORT=""
ERASE_FLASH="no"

usage() {
  printf '%s\n' \
    'Usage:' \
    '  ./bootload.sh [--port DEVICE] [--erase]' \
    '' \
    'Options:' \
    '  --port DEVICE  Select one ESP32-S3 serial port.' \
    '  --erase        Erase the complete Flash, including Mesh keys/settings.' \
    '' \
    'Without --erase, existing Provisioning and Mesh configuration remain in NVS.'
}

fail() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

print_stage() {
  CURRENT_STAGE="$1"
  printf '\n[%s] STAGE=%s\n' \
    "$(TZ=Asia/Seoul date '+%Y-%m-%d %H:%M:%S KST')" "$CURRENT_STAGE"
}

finish() {
  local exit_code=$?
  trap - EXIT
  if [[ "$RESULT" == "PASS" && $exit_code -eq 0 ]]; then
    printf '\nRESULT=PASS\n'
    printf 'STAGE=complete\n'
    printf 'BUILD=PASS\nFLASH=PASS\nRUNTIME=PASS\n'
    printf 'IPHONE_DISCOVERY=NOT_VERIFIED\n'
    printf 'PROVISIONING=NOT_VERIFIED\nCONFIGURATION=NOT_VERIFIED\n'
    printf 'GROUP_ONOFF=NOT_VERIFIED\nTRIPLET_RELAY=NOT_VERIFIED\n'
  else
    ((exit_code != 0)) || exit_code=1
    printf '\nRESULT=FAIL\nSTAGE=%s\nEXIT_CODE=%d\n' \
      "$CURRENT_STAGE" "$exit_code"
  fi
  printf 'PORT=%s\nLOG_FILE=%s\n' "${PORT:-not-selected}" "$LOG_FILE"
  exit "$exit_code"
}

while (($# > 0)); do
  case "$1" in
  --port)
    (($# >= 2)) || fail '--port requires a device path.'
    PORT="$2"
    shift 2
    ;;
  --erase)
    ERASE_FLASH="yes"
    shift
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    fail "Unknown argument: $1"
    ;;
  esac
done

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1
trap finish EXIT

printf 'ESP32-S3 Layer 7 standard Bluetooth Mesh one-board workflow\n'
printf 'PROJECT_DIR=%s\nERASE_FLASH=%s\n' "$PROJECT_DIR" "$ERASE_FLASH"

print_stage "toolchain_check"
for command_name in find grep sort tee; do
  command -v "$command_name" >/dev/null 2>&1 || \
    fail "Required command is unavailable: $command_name"
done
if [[ -n "${ESP_IDF_PATH:-}" ]]; then
  IDF_INSTALL_DIR="$ESP_IDF_PATH"
elif [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
  IDF_INSTALL_DIR="$IDF_PATH"
else
  IDF_INSTALL_DIR="$DEFAULT_IDF_DIR"
fi
[[ -f "$IDF_INSTALL_DIR/export.sh" ]] || \
  fail "ESP-IDF export.sh not found: $IDF_INSTALL_DIR/export.sh"
# shellcheck disable=SC1091
source "$IDF_INSTALL_DIR/export.sh"
for command_name in idf.py esptool.py python; do
  command -v "$command_name" >/dev/null 2>&1 || \
    fail "$command_name is unavailable after ESP-IDF export."
done
idf.py --version

print_stage "serial_device_detection"
serial_ports=()
while IFS= read -r serial_port; do
  serial_ports+=("$serial_port")
done < <(
  find /dev -maxdepth 1 \
    \( -name 'cu.usbmodem*' -o -name 'cu.usbserial*' \
    -o -name 'cu.SLAB_USBtoUART*' -o -name 'cu.wchusbserial*' \) \
    -print 2>/dev/null | sort
)
printf 'Detected USB serial ports:\n'
((${#serial_ports[@]} > 0)) && printf '  %s\n' "${serial_ports[@]}" || printf '  none\n'
if [[ -z "$PORT" ]]; then
  ((${#serial_ports[@]} == 1)) || \
    fail 'Auto-selection requires exactly one serial port. Use --port DEVICE.'
  PORT="${serial_ports[0]}"
fi
[[ -c "$PORT" ]] || fail "Selected serial port is not a character device: $PORT"
printf 'SELECTED_PORT=%s\n' "$PORT"

print_stage "chip_profile"
chip_output="$(esptool.py --chip "$EXPECTED_TARGET" --port "$PORT" \
  --baud "$MONITOR_BAUD" --before default_reset --after hard_reset \
  --no-stub chip_id)"
printf '%s\n' "$chip_output"
grep -q 'ESP32-S3' <<<"$chip_output" || fail 'Connected chip is not ESP32-S3.'
flash_output="$(esptool.py --chip "$EXPECTED_TARGET" --port "$PORT" \
  --baud "$MONITOR_BAUD" --before default_reset --after hard_reset \
  --no-stub flash_id)"
printf '%s\n' "$flash_output"
grep -Eiq "Detected flash size:[[:space:]]*$EXPECTED_FLASH_SIZE" \
  <<<"$flash_output" || fail "Expected $EXPECTED_FLASH_SIZE Flash was not detected."

print_stage "build"
cd "$PROJECT_DIR"
if [[ ! -f sdkconfig ]] || \
  ! grep -qx 'CONFIG_IDF_TARGET="esp32s3"' sdkconfig; then
  idf.py set-target "$EXPECTED_TARGET"
fi
idf.py build
for artifact in build/bootloader/bootloader.bin \
  build/partition_table/partition-table.bin build/esp32s3_layer_7.bin; do
  [[ -f "$artifact" ]] || fail "Missing build artifact: $artifact"
  printf 'BUILD_ARTIFACT=%s\n' "$artifact"
done

if [[ "$ERASE_FLASH" == "yes" ]]; then
  print_stage "erase_flash"
  esptool.py --chip "$EXPECTED_TARGET" --port "$PORT" erase_flash
fi

print_stage "flash"
idf.py -p "$PORT" flash

print_stage "runtime_verification"
python - "$PORT" "$MONITOR_BAUD" "$MONITOR_TIMEOUT_SECONDS" <<'PY'
import sys
import time

import serial

port = sys.argv[1]
baud = int(sys.argv[2])
deadline = time.monotonic() + int(sys.argv[3])
required = {
    "boot": "[LAYER-7] BOOT_SUCCESS",
    "identity": "[LAYER-7] NODE_IDENTITY",
    "mesh": "[LAYER-7] MESH_INITIALIZED",
    "active": "[LAYER-7] NODE_ACTIVE",
    "status": "[LAYER-7] STATUS",
}
seen = set()

with serial.Serial(port, baudrate=baud, timeout=0.25) as device:
    device.dtr = False
    device.rts = True
    time.sleep(0.1)
    device.rts = False
    while time.monotonic() < deadline:
        raw = device.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        print(f"[SERIAL] {line}", flush=True)
        for key, marker in required.items():
            if marker in line:
                seen.add(key)
        if seen == set(required):
            print("LOCAL_RUNTIME_MARKERS=PASS", flush=True)
            raise SystemExit(0)

print("LOCAL_RUNTIME_MARKERS=FAIL", file=sys.stderr, flush=True)
print("MISSING_MARKERS=" + ",".join(sorted(set(required) - seen)),
      file=sys.stderr, flush=True)
raise SystemExit(1)
PY

RESULT="PASS"
CURRENT_STAGE="complete"
