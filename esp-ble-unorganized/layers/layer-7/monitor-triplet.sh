#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
LOG_DIR="$SCRIPT_DIR/logs"
RUN_TIMESTAMP="$(TZ=Asia/Seoul date '+%Y%m%dT%H%M%S-KST')"
LOG_FILE="$LOG_DIR/esp32s3-layer-7-triplet-monitor-$RUN_TIMESTAMP.log"
PORT_A="${1:-}"
PORT_B="${2:-}"
PORT_C="${3:-}"

if (($# != 3)); then
  printf 'Usage: ./monitor-triplet.sh PORT_A PORT_B PORT_C\n' >&2
  exit 2
fi
for port in "$PORT_A" "$PORT_B" "$PORT_C"; do
  [[ -c "$port" ]] || { printf 'Invalid serial port: %s\n' "$port" >&2; exit 2; }
done

mkdir -p "$LOG_DIR"
printf 'Interactive input: A:on, B:status, C:factory-reset, or quit\n'
printf 'LOG_FILE=%s\n' "$LOG_FILE"

python - "$PORT_A" "$PORT_B" "$PORT_C" <<'PY' | tee -a "$LOG_FILE"
import select
import sys
import time

import serial

ports = dict(zip("ABC", sys.argv[1:4]))
devices = {label: serial.Serial(port, 115200, timeout=0)
           for label, port in ports.items()}
buffers = {label: bytearray() for label in devices}
markers = {
    "provisioning": "PROVISIONING_COMPLETE",
    "configuration": "CLIENT_PUBLICATION_READY",
    "transmit": "ONOFF_TX_ACCEPTED",
    "receive": "ONOFF_RX",
    "relay": "RELAY_STATE_CHANGED",
}
seen = set()

try:
    while True:
        readable, _, _ = select.select([sys.stdin], [], [], 0.05)
        if readable:
            text = sys.stdin.readline()
            if text == "" or text.strip() == "quit":
                break
            if ":" not in text:
                print("INPUT_ERROR expected LABEL:command", flush=True)
            else:
                label, command = text.split(":", 1)
                label = label.strip().upper()
                if label not in devices:
                    print("INPUT_ERROR label must be A, B, or C", flush=True)
                else:
                    devices[label].write((command.strip() + "\n").encode())
                    print(f"[HOST -> {label}] {command.strip()}", flush=True)

        for label, device in devices.items():
            chunk = device.read(device.in_waiting or 1)
            if not chunk:
                continue
            buffers[label].extend(chunk)
            while b"\n" in buffers[label]:
                raw, _, rest = buffers[label].partition(b"\n")
                buffers[label] = bytearray(rest)
                line = raw.rstrip(b"\r").decode("utf-8", errors="replace")
                print(f"[{label} {ports[label]}] {line}", flush=True)
                for stage, marker in markers.items():
                    if marker in line:
                        seen.add(stage)
finally:
    for device in devices.values():
        device.close()
    print("OBSERVED_STAGES=" + ",".join(sorted(seen)), flush=True)
    print("RELAY_PATH_PROOF=NOT_INFERRED_FROM_CALLBACKS", flush=True)
PY
