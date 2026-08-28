#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$SCRIPT_DIR"
LOG_DIR="$PROJECT_DIR/logs"
RUN_TIMESTAMP="$(TZ=Asia/Seoul date '+%Y%m%dT%H%M%S-KST')"
LOG_FILE="$LOG_DIR/esp32s3-layer-4-dual-role-$RUN_TIMESTAMP.log"
DEFAULT_IDF_DIR="/Users/kafka/esp/esp-idf-v5.5.5"
EXPECTED_TARGET="esp32s3"
EXPECTED_FLASH_SIZE="16MB"
MONITOR_BAUD="115200"
MONITOR_TIMEOUT_SECONDS="25"
RUNTIME_MARKER="[LAYER-4] DUAL_ROLE_ACTIVE"

CURRENT_STAGE="initialization"
RESULT="FAIL"
PORT=""

usage() {
  printf '%s\n' \
    'Usage:' \
    '  ./bootload.sh' \
    '  ./bootload.sh --port /dev/cu.usbmodemXXXXXXXX' \
    '' \
    'This verifies one board local runtime. Use bootload-pair.sh for PEER_RX.' \
    '' \
    'Environment:' \
    '  ESP_IDF_PATH  Optional ESP-IDF installation path override.'
}

finish() {
  local exit_code=$?
  trap - EXIT
  if [[ "$RESULT" == "PASS" && $exit_code -eq 0 ]]; then
    printf '\nRESULT=PASS\n'
    printf 'STAGE=complete\n'
    printf 'LOCAL_GATT_ADV_SCAN=PASS\n'
    printf 'PAIR_PEER_RX=NOT_VERIFIED\n'
  else
    if ((exit_code == 0)); then
      exit_code=1
    fi
    printf '\nRESULT=FAIL\n'
    printf 'STAGE=%s\n' "$CURRENT_STAGE"
    printf 'EXIT_CODE=%d\n' "$exit_code"
  fi
  printf 'LOG_FILE=%s\n' "$LOG_FILE"
  exit "$exit_code"
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

while (($# > 0)); do
  case "$1" in
    --port)
      (($# >= 2)) || fail '--port requires a device path.'
      PORT="$2"
      shift 2
      ;;
    -h|--help)
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

printf 'ESP32-S3 Layer 4 one-board dual-role workflow\n'
printf 'PROJECT_DIR=%s\n' "$PROJECT_DIR"
printf 'RUN_TIMESTAMP=%s\n' "$RUN_TIMESTAMP"

print_stage "toolchain_check"
for required_command in find ioreg awk grep sort tee; do
  command -v "$required_command" >/dev/null 2>&1 || \
    fail "Required command is not installed: $required_command"
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
printf 'IDF_INSTALL_DIR=%s\n' "$IDF_INSTALL_DIR"
# shellcheck disable=SC1091
source "$IDF_INSTALL_DIR/export.sh"

for required_command in idf.py esptool.py python; do
  command -v "$required_command" >/dev/null 2>&1 || \
    fail "$required_command is unavailable after ESP-IDF export."
done
idf.py --version
esptool.py version
python -c 'import serial; print("pyserial=" + serial.VERSION)'

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
if ((${#serial_ports[@]} == 0)); then
  printf '  none\n'
else
  printf '  %s\n' "${serial_ports[@]}"
fi
if [[ -z "$PORT" ]]; then
  ((${#serial_ports[@]} > 0)) || fail 'No USB serial port was found.'
  ((${#serial_ports[@]} == 1)) || \
    fail 'Multiple USB serial ports found. Re-run with --port DEVICE.'
  PORT="${serial_ports[0]}"
fi
[[ -e "$PORT" ]] || fail "Selected serial port does not exist: $PORT"
[[ -c "$PORT" ]] || fail "Selected serial path is not a character device: $PORT"
printf 'SELECTED_PORT=%s\n' "$PORT"

print_stage "usb_device_profile"
ioreg -p IOUSB -w 0 -l | awk '
  /[+]\-o / {
    device = $0
    sub(/^.*[+]\-o /, "", device)
    sub(/  <class.*$/, "", device)
  }
  /"USB Product Name"|"USB Serial Number"|"idVendor"|"idProduct"/ {
    if (device != last_device) {
      printf "\nUSB_DEVICE=%s\n", device
      last_device = device
    }
    line = $0
    sub(/^[[:space:]|]*/, "", line)
    printf "%s\n", line
  }
'

print_stage "chip_profile"
chip_output="$(esptool.py --chip "$EXPECTED_TARGET" --port "$PORT" \
  --baud "$MONITOR_BAUD" --before default_reset --after hard_reset \
  --no-stub chip_id)"
printf '%s\n' "$chip_output"
printf '%s\n' "$chip_output" | grep -q 'ESP32-S3' || \
  fail 'Connected chip did not identify as ESP32-S3.'
flash_output="$(esptool.py --chip "$EXPECTED_TARGET" --port "$PORT" \
  --baud "$MONITOR_BAUD" --before default_reset --after hard_reset \
  --no-stub flash_id)"
printf '%s\n' "$flash_output"
printf '%s\n' "$flash_output" | \
  grep -Eiq "Detected flash size:[[:space:]]*$EXPECTED_FLASH_SIZE" || \
  fail "Expected $EXPECTED_FLASH_SIZE SPI flash was not detected."

print_stage "target_configuration"
cd "$PROJECT_DIR"
if [[ ! -f sdkconfig ]] || \
   ! grep -qx 'CONFIG_IDF_TARGET="esp32s3"' sdkconfig; then
  idf.py set-target "$EXPECTED_TARGET"
else
  printf 'Existing sdkconfig target is already %s.\n' "$EXPECTED_TARGET"
fi

print_stage "build"
idf.py build
for build_artifact in \
  build/bootloader/bootloader.bin \
  build/partition_table/partition-table.bin \
  build/esp32s3_layer_4.bin; do
  [[ -f "$build_artifact" ]] || fail "Missing build artifact: $build_artifact"
  printf 'BUILD_ARTIFACT=%s\n' "$build_artifact"
done

print_stage "flash"
idf.py -p "$PORT" flash

print_stage "local_runtime_verification"
printf 'Waiting up to %s seconds for %s...\n' \
  "$MONITOR_TIMEOUT_SECONDS" "$RUNTIME_MARKER"
if ! python - "$PORT" "$MONITOR_BAUD" \
  "$MONITOR_TIMEOUT_SECONDS" "$RUNTIME_MARKER" <<'PY'
import sys
import time
import serial

port = sys.argv[1]
baud = int(sys.argv[2])
timeout_seconds = int(sys.argv[3])
marker = sys.argv[4]
deadline = time.monotonic() + timeout_seconds

try:
    with serial.Serial(port, baudrate=baud, timeout=0.25) as device:
        device.dtr = False
        device.rts = False
        while time.monotonic() < deadline:
            raw_line = device.readline()
            if not raw_line:
                continue
            line = raw_line.decode("utf-8", errors="replace").rstrip("\r\n")
            print(f"[SERIAL] {line}", flush=True)
            if marker in line and "gatt=yes" in line and \
                    "advertising=yes" in line and "scanning=yes" in line:
                print("LOCAL_RUNTIME_MARKER_FOUND=yes", flush=True)
                raise SystemExit(0)
except serial.SerialException as error:
    print(f"SERIAL_ERROR={error}", file=sys.stderr, flush=True)
    raise SystemExit(2)

print("LOCAL_RUNTIME_MARKER_FOUND=no", file=sys.stderr, flush=True)
raise SystemExit(1)
PY
then
  fail "Local dual-role runtime marker was not received from $PORT."
fi

RESULT="PASS"
CURRENT_STAGE="complete"
