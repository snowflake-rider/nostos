#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$SCRIPT_DIR"
LOG_DIR="$PROJECT_DIR/logs"
RUN_TIMESTAMP="$(TZ=Asia/Seoul date '+%Y%m%dT%H%M%S-KST')"
LOG_FILE="$LOG_DIR/esp32s3-layer-6-relay-node-triplet-$RUN_TIMESTAMP.log"
DEFAULT_IDF_DIR="/Users/kafka/esp/esp-idf-v5.5.5"
EXPECTED_TARGET="esp32s3"
EXPECTED_FLASH_SIZE="16MB"
MONITOR_BAUD="115200"
TRIPLET_TIMEOUT_SECONDS="120"

CURRENT_STAGE="initialization"
RESULT="FAIL"
PORT_A=""
PORT_B=""
PORT_C=""

usage() {
  printf '%s\n' \
    'Usage:' \
    '  ./bootload-triplet.sh' \
    '  ./bootload-triplet.sh --port-a /dev/cu.usbmodemA --port-b /dev/cu.usbmodemB --port-c /dev/cu.usbmodemC' \
    '' \
    'With no port arguments, exactly three supported USB serial ports are required.' \
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
    printf 'TRIPLET_ADV_SCAN=PASS\n'
    printf 'TRIPLET_GATT_SERVER=PASS\n'
    printf 'TRIPLET_ORIGIN_TX=PASS\n'
    printf 'TRIPLET_DIRECT_RX=PASS\n'
    printf 'TRIPLET_FORWARD_TX=PASS\n'
    printf 'TRIPLET_RELAYED_RX=PASS\n'
    printf 'TRIPLET_PACKET_CRC=PASS\n'
    printf 'TRIPLET_RELAY=PASS\n'
  else
    if ((exit_code == 0)); then
      exit_code=1
    fi
    printf '\nRESULT=FAIL\n'
    printf 'STAGE=%s\n' "$CURRENT_STAGE"
    printf 'EXIT_CODE=%d\n' "$exit_code"
  fi
  printf 'PORT_A=%s\n' "${PORT_A:-not-selected}"
  printf 'PORT_B=%s\n' "${PORT_B:-not-selected}"
  printf 'PORT_C=%s\n' "${PORT_C:-not-selected}"
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
    --port-a)
      (($# >= 2)) || fail '--port-a requires a device path.'
      PORT_A="$2"
      shift 2
      ;;
    --port-b)
      (($# >= 2)) || fail '--port-b requires a device path.'
      PORT_B="$2"
      shift 2
      ;;
    --port-c)
      (($# >= 2)) || fail '--port-c requires a device path.'
      PORT_C="$2"
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

printf 'ESP32-S3 Layer 6 three-board relay-node workflow\n'
printf 'PROJECT_DIR=%s\n' "$PROJECT_DIR"
printf 'RUN_TIMESTAMP=%s\n' "$RUN_TIMESTAMP"

print_stage "toolchain_check"
for required_command in find grep sort tee; do
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

print_stage "serial_triplet_detection"
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

if [[ -z "$PORT_A" && -z "$PORT_B" && -z "$PORT_C" ]]; then
  ((${#serial_ports[@]} == 3)) || \
    fail 'Auto-selection requires exactly three USB serial ports.'
  PORT_A="${serial_ports[0]}"
  PORT_B="${serial_ports[1]}"
  PORT_C="${serial_ports[2]}"
elif [[ -z "$PORT_A" || -z "$PORT_B" || -z "$PORT_C" ]]; then
  fail 'Specify --port-a, --port-b, and --port-c together, or specify none.'
fi

[[ "$PORT_A" != "$PORT_B" && "$PORT_A" != "$PORT_C" && "$PORT_B" != "$PORT_C" ]] || \
  fail 'Board A, Board B, and Board C ports must all differ.'
for selected_port in "$PORT_A" "$PORT_B" "$PORT_C"; do
  [[ -e "$selected_port" ]] || fail "Serial port does not exist: $selected_port"
  [[ -c "$selected_port" ]] || \
    fail "Serial path is not a character device: $selected_port"
done
printf 'SELECTED_PORT_A=%s\n' "$PORT_A"
printf 'SELECTED_PORT_B=%s\n' "$PORT_B"
printf 'SELECTED_PORT_C=%s\n' "$PORT_C"

profile_device() {
  local label="$1"
  local port="$2"
  local chip_output
  local flash_output

  printf '\nPROFILE_BOARD_%s port=%s\n' "$label" "$port"
  chip_output="$(esptool.py --chip "$EXPECTED_TARGET" --port "$port" \
    --baud "$MONITOR_BAUD" --before default_reset --after hard_reset \
    --no-stub chip_id)"
  printf '%s\n' "$chip_output"
  printf '%s\n' "$chip_output" | grep -q 'ESP32-S3' || \
    fail "Board $label did not identify as ESP32-S3."

  flash_output="$(esptool.py --chip "$EXPECTED_TARGET" --port "$port" \
    --baud "$MONITOR_BAUD" --before default_reset --after hard_reset \
    --no-stub flash_id)"
  printf '%s\n' "$flash_output"
  printf '%s\n' "$flash_output" | \
    grep -Eiq "Detected flash size:[[:space:]]*$EXPECTED_FLASH_SIZE" || \
    fail "Board $label did not report $EXPECTED_FLASH_SIZE SPI flash."
}

print_stage "triplet_chip_profile"
profile_device A "$PORT_A"
profile_device B "$PORT_B"
profile_device C "$PORT_C"

print_stage "target_configuration"
cd "$PROJECT_DIR"
if [[ ! -f sdkconfig ]] || \
   ! grep -qx 'CONFIG_IDF_TARGET="esp32s3"' sdkconfig; then
  idf.py set-target "$EXPECTED_TARGET"
else
  printf 'Existing sdkconfig target is already %s.\n' "$EXPECTED_TARGET"
fi

print_stage "build_once"
idf.py build
for build_artifact in \
  build/bootloader/bootloader.bin \
  build/partition_table/partition-table.bin \
  build/esp32s3_layer_6.bin; do
  [[ -f "$build_artifact" ]] || fail "Missing build artifact: $build_artifact"
  printf 'BUILD_ARTIFACT=%s\n' "$build_artifact"
done

print_stage "flash_board_a"
idf.py -p "$PORT_A" flash

print_stage "flash_board_b"
idf.py -p "$PORT_B" flash

print_stage "flash_board_c"
idf.py -p "$PORT_C" flash

print_stage "triplet_relay_chain_verification"
printf 'Waiting up to %s seconds for A origin -> B direct RX/forward -> C relayed RX...\n' \
  "$TRIPLET_TIMEOUT_SECONDS"
if ! python - "$PORT_A" "$PORT_B" "$PORT_C" "$MONITOR_BAUD" \
  "$TRIPLET_TIMEOUT_SECONDS" <<'PY'
import re
import sys
import time
import serial

port_a, port_b, port_c = sys.argv[1], sys.argv[2], sys.argv[3]
baud = int(sys.argv[4])
timeout_seconds = int(sys.argv[5])
deadline = time.monotonic() + timeout_seconds

node_pattern = re.compile(r"RELAY_NODE_ACTIVE .* node=([0-9A-F]{2}) ")
origin_pattern = re.compile(
    r"PACKET_TX_ORIGIN local=([0-9A-F]{2}) origin=([0-9A-F]{2}) "
    r"seq=([0-9]+) ttl=2 .*payload=HELLO crc=ok"
)
direct_pattern = re.compile(
    r"PACKET_RX_DIRECT local=([0-9A-F]{2}) origin=([0-9A-F]{2}) "
    r"via=([0-9A-F]{2}) seq=([0-9]+) ttl=2 .*"
    r"duplicate=no crc=ok"
)
forward_pattern = re.compile(
    r"PACKET_TX_FORWARD local=([0-9A-F]{2}) origin=([0-9A-F]{2}) "
    r"seq=([0-9]+) ttl=1 .*payload=HELLO crc=ok"
)
relayed_pattern = re.compile(
    r"PACKET_RX_RELAYED local=([0-9A-F]{2}) origin=([0-9A-F]{2}) "
    r"via=([0-9A-F]{2}) seq=([0-9]+) ttl=1 .*crc=ok"
)

state = {
    "A": {"ready": False, "node": None},
    "B": {"ready": False, "node": None},
    "C": {"ready": False, "node": None},
}
origins = set()
directs = set()
forwards = set()
relayed = set()


def record(label, line):
    print(f"[{label}] {line}", flush=True)
    node_match = node_pattern.search(line)
    if node_match and all(token in line for token in (
        "gatt=yes", "advertising=yes", "scanning=yes"
    )):
        state[label]["ready"] = True
        state[label]["node"] = node_match.group(1)

    for pattern, destination in (
        (origin_pattern, origins),
        (direct_pattern, directs),
        (forward_pattern, forwards),
        (relayed_pattern, relayed),
    ):
        match = pattern.search(line)
        if match:
            values = match.groups()
            state[label]["node"] = state[label]["node"] or values[0]
            if destination is origins:
                local, origin, sequence = values
                if local == origin:
                    destination.add((origin, int(sequence)))
            elif destination is directs:
                local, origin, via, sequence = values
                if origin == via:
                    destination.add((local, origin, int(sequence)))
            elif destination is forwards:
                local, origin, sequence = values
                destination.add((local, origin, int(sequence)))
            else:
                local, origin, via, sequence = values
                destination.add((local, origin, via, int(sequence)))


def find_chain():
    nodes = {item["node"] for item in state.values()}
    if None in nodes or len(nodes) != 3:
        return None
    for origin, sequence in origins:
        for relay in nodes - {origin}:
            if (relay, origin, sequence) not in directs:
                continue
            if (relay, origin, sequence) not in forwards:
                continue
            for receiver in nodes - {origin, relay}:
                if (receiver, origin, relay, sequence) in relayed:
                    return origin, relay, receiver, sequence
    return None


def complete():
    return all(item["ready"] for item in state.values()) and find_chain() is not None


devices = {}
try:
    for label, port in (("A", port_a), ("B", port_b), ("C", port_c)):
        device = serial.Serial(port, baudrate=baud, timeout=0.04)
        device.dtr = False
        device.rts = False
        devices[label] = device

    while time.monotonic() < deadline:
        for label, device in devices.items():
            raw_line = device.readline()
            if raw_line:
                record(
                    label,
                    raw_line.decode("utf-8", errors="replace").rstrip("\r\n"),
                )
        if complete():
            origin, relay, receiver, sequence = find_chain()
            print(f"RELAY_CHAIN_ORIGIN={origin}", flush=True)
            print(f"RELAY_CHAIN_FORWARDER={relay}", flush=True)
            print(f"RELAY_CHAIN_RECEIVER={receiver}", flush=True)
            print(f"RELAY_CHAIN_SEQUENCE={sequence}", flush=True)
            print("TRIPLET_RELAY_CHAIN=yes", flush=True)
            raise SystemExit(0)
except serial.SerialException as error:
    print(f"SERIAL_ERROR={error}", file=sys.stderr, flush=True)
    raise SystemExit(2)
finally:
    for device in devices.values():
        device.close()

print(f"TRIPLET_STATE={state}", file=sys.stderr, flush=True)
print(f"ORIGINS={sorted(origins)}", file=sys.stderr, flush=True)
print(f"DIRECTS={sorted(directs)}", file=sys.stderr, flush=True)
print(f"FORWARDS={sorted(forwards)}", file=sys.stderr, flush=True)
print(f"RELAYED={sorted(relayed)}", file=sys.stderr, flush=True)
print("TRIPLET_RELAY_CHAIN=no", file=sys.stderr, flush=True)
raise SystemExit(1)
PY
then
  fail 'Three boards did not prove one exact origin -> direct RX -> forward TX -> relayed RX identity chain.'
fi

RESULT="PASS"
CURRENT_STAGE="complete"
