#!/usr/bin/env bash
# Install v2 through one halted GDB session; preserve the checkpoint journal.
set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
REPO_DIR="$(cd -- "$PROJECT_DIR/../.." && pwd -P)"
EXPECTED_SERIAL="066DFF485277504867161930"
EXPECTED_FLASH_BYTES=524288
APP_LIMIT_BYTES=$((256 * 1024))
GDB_PORT=4243
ELF="$PROJECT_DIR/build-v2-release/nostos_stm32.elf"
OUTPUT_DIR=""
STUTIL_PID=""
CAPTURE_PID=""
FLASH_PHASE="preflight"

IMAGE_BASENAME="nostos-stm32-v2.bin"
BACKUP_BASENAME="before-full-flash.bin"
JOURNAL_BEFORE_BASENAME="before-checkpoint-journal.bin"
JOURNAL_AFTER_BASENAME="after-checkpoint-journal-before-boot.bin"
APP_READBACK_BASENAME="after-app-readback.bin"
GDB_COMMANDS_BASENAME="install.gdb"
BOOT_CAPTURE_BASENAME="capture-boot.py"
BOOT_TRIGGER_BASENAME="post-verify-reset.trigger"
BOOT_ACK_BASENAME="post-verify-reset.armed"

usage() {
    printf 'usage: %s [--output-dir PATH]\n' "$0" >&2
}

while (($#)); do
    case "$1" in
        --output-dir)
            (($# >= 2)) || { usage; exit 64; }
            OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) usage; exit 64 ;;
    esac
done

if ((EUID != 0)); then
    printf 'STM32_FLASH_REQUIRES_ADMIN: macOS USB capture needs root privilege\n' >&2
    exit 77
fi

ST_INFO="$(command -v st-info || true)"
ST_UTIL="$(command -v st-util || true)"
OBJCOPY="$(command -v arm-none-eabi-objcopy || true)"
GDB="$(command -v arm-none-eabi-gdb || true)"
READELF="$(command -v arm-none-eabi-readelf || true)"
IOREG="/usr/sbin/ioreg"
PYTHON="/usr/bin/python3"
[[ -n "$ST_INFO" ]] || ST_INFO=/opt/homebrew/bin/st-info
[[ -n "$ST_UTIL" ]] || ST_UTIL=/opt/homebrew/bin/st-util
if [[ -z "$OBJCOPY" || -z "$GDB" || -z "$READELF" ]]; then
    compiler_ar="$(sed -n 's#^CMAKE_C_COMPILER_AR:FILEPATH=##p' \
        "$PROJECT_DIR/build-v2-release/CMakeCache.txt" 2>/dev/null | sed -n '1p')"
    if [[ -n "$compiler_ar" ]]; then
        tool_dir="${compiler_ar%/arm-none-eabi-gcc-ar}"
        [[ -n "$OBJCOPY" ]] || OBJCOPY="$tool_dir/arm-none-eabi-objcopy"
        [[ -n "$GDB" ]] || GDB="$tool_dir/arm-none-eabi-gdb"
        [[ -n "$READELF" ]] || READELF="$tool_dir/arm-none-eabi-readelf"
    fi
fi
for tool in "$ST_INFO" "$ST_UTIL" "$OBJCOPY" "$GDB" "$READELF" "$IOREG" "$PYTHON"; do
    [[ -x "$tool" ]] || { printf 'STM32_FLASH_TOOL_MISSING: %s\n' "$tool" >&2; exit 69; }
done
[[ -f "$ELF" ]] || { printf 'STM32_ELF_MISSING: %s\n' "$ELF" >&2; exit 66; }

if [[ -z "$OUTPUT_DIR" ]]; then
    stamp="$(date '+%Y%m%dT%H%M%S%Z')"
    OUTPUT_DIR="$REPO_DIR/build/hardware-results/stm32-v2-$stamp"
fi
case "$OUTPUT_DIR" in
    "$REPO_DIR"/build/hardware-results/*) ;;
    *) printf 'UNSAFE_OUTPUT_DIRECTORY: %s\n' "$OUTPUT_DIR" >&2; exit 64 ;;
esac
[[ ! -e "$OUTPUT_DIR" ]] || {
    printf 'OUTPUT_DIRECTORY_ALREADY_EXISTS: %s\n' "$OUTPUT_DIR" >&2; exit 73;
}

# Resolve the VCP from the exact ST-Link USB device.  ioreg's plist preserves
# parent/child relationships, unlike matching a global /dev/cu.usbmodem glob.
resolve_vcp_callout() {
    "$IOREG" -a -l -w 0 | EXPECTED_SERIAL="$EXPECTED_SERIAL" "$PYTHON" -c '
import os
import plistlib
import stat
import sys

expected = os.environ["EXPECTED_SERIAL"]
roots = plistlib.load(sys.stdin.buffer)
matches = []

def children(node):
    if not isinstance(node, dict):
        return []
    value = node.get("IORegistryEntryChildren", [])
    return value if isinstance(value, list) else []

def walk(node):
    if not isinstance(node, dict):
        return
    if node.get("USB Serial Number") == expected:
        matches.append(node)
    for child in children(node):
        walk(child)

for root in roots if isinstance(roots, list) else [roots]:
    walk(root)

callouts = set()
def collect_callouts(node):
    if not isinstance(node, dict):
        return
    value = node.get("IOCalloutDevice")
    if isinstance(value, str) and value.startswith("/dev/cu."):
        callouts.add(value)
    for child in children(node):
        collect_callouts(child)

for match in matches:
    collect_callouts(match)

if len(matches) != 1 or len(callouts) != 1:
    print(
        f"STM32_VCP_MAPPING_AMBIGUOUS: serial_nodes={len(matches)} "
        f"callouts={sorted(callouts)!r}",
        file=sys.stderr,
    )
    raise SystemExit(1)

callout = next(iter(callouts))
try:
    mode = os.stat(callout).st_mode
except OSError as exc:
    print(f"STM32_VCP_CALLOUT_UNAVAILABLE: {callout}: {exc}", file=sys.stderr)
    raise SystemExit(1)
if not stat.S_ISCHR(mode):
    print(f"STM32_VCP_CALLOUT_NOT_CHARACTER_DEVICE: {callout}", file=sys.stderr)
    raise SystemExit(1)
print(callout)
'
}
if ! VCP_CALLOUT="$(resolve_vcp_callout)"; then
    printf 'STM32_VCP_MAPPING_FAILED: serial=%s\n' "$EXPECTED_SERIAL" >&2
    exit 65
fi
case "$VCP_CALLOUT" in
    /dev/cu.*) ;;
    *) printf 'STM32_VCP_MAPPING_INVALID: %s\n' "$VCP_CALLOUT" >&2; exit 65 ;;
esac
VCP_DIALIN="/dev/tty.${VCP_CALLOUT#/dev/cu.}"

mkdir -p -- "$OUTPUT_DIR"
chmod 700 "$OUTPUT_DIR"

IMAGE="$OUTPUT_DIR/$IMAGE_BASENAME"
BACKUP="$OUTPUT_DIR/$BACKUP_BASENAME"
JOURNAL_BEFORE="$OUTPUT_DIR/$JOURNAL_BEFORE_BASENAME"
JOURNAL_AFTER="$OUTPUT_DIR/$JOURNAL_AFTER_BASENAME"
APP_READBACK="$OUTPUT_DIR/$APP_READBACK_BASENAME"
GDB_COMMANDS="$OUTPUT_DIR/$GDB_COMMANDS_BASENAME"
GDB_LOG="$OUTPUT_DIR/gdb.log"
STUTIL_LOG="$OUTPUT_DIR/st-util.log"
BOOT_CAPTURE="$OUTPUT_DIR/$BOOT_CAPTURE_BASENAME"
BOOT_LOG="$OUTPUT_DIR/boot.log"
BOOT_TRIGGER="$OUTPUT_DIR/$BOOT_TRIGGER_BASENAME"
BOOT_ACK="$OUTPUT_DIR/$BOOT_ACK_BASENAME"
MANIFEST="$OUTPUT_DIR/result.txt"

stop_stutil() {
    local pid="${STUTIL_PID:-}"
    local attempt
    [[ -n "$pid" ]] || return 0

    if kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null || true
        for attempt in {1..30}; do
            kill -0 "$pid" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
    fi
    wait "$pid" 2>/dev/null || true
    STUTIL_PID=""
}

cleanup() {
    local status=$?
    stop_stutil
    if [[ -n "$CAPTURE_PID" ]] && kill -0 "$CAPTURE_PID" 2>/dev/null; then
        kill "$CAPTURE_PID" 2>/dev/null || true
        wait "$CAPTURE_PID" 2>/dev/null || true
    fi
    chmod 600 "$OUTPUT_DIR"/* 2>/dev/null || true
    local owner
    owner="$(stat -f '%Su' "$REPO_DIR" 2>/dev/null || true)"
    if [[ -n "$owner" && "$owner" != root ]]; then
        chown -R "$owner" "$OUTPUT_DIR" 2>/dev/null || true
    fi
    if ((status != 0)) && [[ "$FLASH_PHASE" == "debugger" ]]; then
        printf 'STM32_TARGET_STATE=UNVERIFIED_AFTER_HALTED_SESSION; DO_NOT_POWER_CYCLE_OR_RESET\n' >&2
        printf 'Recover or inspect with ST-LINK before attempting another install.\n' >&2
    elif ((status != 0)) && [[ "$FLASH_PHASE" == "boot_wait" ]]; then
        printf 'STM32_TARGET_STATE=RESET_COMPLETED_BUT_RUNTIME_UNVERIFIED\n' >&2
    fi
}
trap cleanup EXIT

for port in "$VCP_CALLOUT" "$VCP_DIALIN"; do
    if [[ -e "$port" ]] && lsof -nP -- "$port" >/dev/null 2>&1; then
        printf 'STM32_VCP_IN_USE: %s\n' "$port" >&2
        exit 73
    fi
done
if lsof -nP -iTCP:"$GDB_PORT" >/dev/null 2>&1; then
    printf 'STM32_GDB_PORT_IN_USE: %s\n' "$GDB_PORT" >&2
    exit 73
fi

probe="$($ST_INFO --probe --connect-under-reset 2>&1)"
serial_lines="$(printf '%s\n' "$probe" | sed -n 's/^[[:space:]]*serial:[[:space:]]*//p')"
serial_count="$(printf '%s\n' "$serial_lines" | sed '/^$/d' | wc -l | tr -d ' ')"
actual_serial="$(printf '%s\n' "$serial_lines" | sed -n '1p' | tr -d '[:space:]')"
actual_flash="$(printf '%s\n' "$probe" | sed -n 's/^[[:space:]]*flash:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | sed -n '1p')"
if [[ "$serial_count" != 1 || "$actual_serial" != "$EXPECTED_SERIAL" ]]; then
    printf 'STM32_IDENTITY_MISMATCH: expected=%s actual=%s count=%s\n' \
        "$EXPECTED_SERIAL" "${actual_serial:-missing}" "$serial_count" >&2
    exit 65
fi
if [[ "$actual_flash" != "$EXPECTED_FLASH_BYTES" ]]; then
    printf 'STM32_FLASH_SIZE_MISMATCH: expected=%s actual=%s\n' \
        "$EXPECTED_FLASH_BYTES" "${actual_flash:-missing}" >&2
    exit 65
fi

"$OBJCOPY" -O binary "$ELF" "$IMAGE"
image_size="$(stat -f '%z' "$IMAGE")"
if ((image_size <= 0 || image_size > APP_LIMIT_BYTES)); then
    printf 'STM32_APP_RANGE_INVALID: bytes=%s limit=%s\n' "$image_size" "$APP_LIMIT_BYTES" >&2
    exit 65
fi
elf_header="$($READELF -h "$ELF")"
grep -F 'Type:                              EXEC (Executable file)' <<<"$elf_header" >/dev/null || {
    printf 'STM32_ELF_TYPE_INVALID\n' >&2; exit 65;
}
grep -F 'Machine:                           ARM' <<<"$elf_header" >/dev/null || {
    printf 'STM32_ELF_MACHINE_INVALID\n' >&2; exit 65;
}
entry_text="$(sed -n 's/^[[:space:]]*Entry point address:[[:space:]]*//p' <<<"$elf_header")"
[[ "$entry_text" =~ ^0x[0-9a-fA-F]+$ ]] || {
    printf 'STM32_ELF_ENTRY_INVALID: %s\n' "${entry_text:-missing}" >&2; exit 65;
}
entry=$((entry_text))
if ((entry < 0x08000000 || entry >= 0x08040000)); then
    printf 'STM32_ELF_ENTRY_INVALID: %s\n' "$entry_text" >&2
    exit 65
fi
load_count=0
vector_load=0
while read -r physical file_size; do
    [[ -n "$physical" && -n "$file_size" ]] || continue
    load_count=$((load_count + 1))
    start=$((physical)); bytes=$((file_size)); end=$((start + bytes))
    ((start == 0x08000000 && bytes > 0)) && vector_load=1
    if ((bytes > 0 && (start < 0x08000000 || end > 0x08040000))); then
        printf 'STM32_ELF_LOAD_OUTSIDE_APP: start=%s size=%s\n' "$physical" "$file_size" >&2
        exit 65
    fi
done < <("$READELF" -lW "$ELF" | awk '$1=="LOAD" {print $4, $5}')
if ((load_count == 0 || vector_load == 0)); then
    printf 'STM32_ELF_LOAD_LAYOUT_INVALID\n' >&2
    exit 65
fi

# Keep one debugger connection from pre-write backup through post-write compare.
# `load` only submits the ELF's flash sections; the linker and binary-size gate
# keep every section below the journal at 0x08040000.
export NOSTOS_FLASH_IMAGE="$IMAGE_BASENAME"
export NOSTOS_FLASH_BACKUP="$BACKUP_BASENAME"
export NOSTOS_JOURNAL_BEFORE="$JOURNAL_BEFORE_BASENAME"
export NOSTOS_JOURNAL_AFTER="$JOURNAL_AFTER_BASENAME"
export NOSTOS_APP_READBACK="$APP_READBACK_BASENAME"
export NOSTOS_IMAGE_SIZE="$image_size"
export NOSTOS_BOOT_TRIGGER="$BOOT_TRIGGER_BASENAME"
export NOSTOS_BOOT_ACK="$BOOT_ACK_BASENAME"
export NOSTOS_VCP_CALLOUT="$VCP_CALLOUT"
{
    printf 'set pagination off\nset confirm off\n'
    printf 'target extended-remote 127.0.0.1:%s\n' "$GDB_PORT"
    printf 'monitor halt\n'
    printf 'dump binary memory "%s" 0x08000000 0x08080000\n' "$BACKUP_BASENAME"
    printf 'dump binary memory "%s" 0x08040000 0x08080000\n' "$JOURNAL_BEFORE_BASENAME"
    printf 'load "%s"\n' "$ELF"
    printf 'monitor halt\n'
    printf 'dump binary memory "%s" 0x08000000 0x%08x\n' \
        "$APP_READBACK_BASENAME" "$((0x08000000 + image_size))"
    printf 'dump binary memory "%s" 0x08040000 0x08080000\n' "$JOURNAL_AFTER_BASENAME"
    cat <<'GDBPY'
python
import os
from pathlib import Path
import gdb
import time
image = Path(os.environ["NOSTOS_FLASH_IMAGE"]).read_bytes()
backup = Path(os.environ["NOSTOS_FLASH_BACKUP"]).read_bytes()
before = Path(os.environ["NOSTOS_JOURNAL_BEFORE"]).read_bytes()
after = Path(os.environ["NOSTOS_JOURNAL_AFTER"]).read_bytes()
readback = Path(os.environ["NOSTOS_APP_READBACK"]).read_bytes()
expected = int(os.environ["NOSTOS_IMAGE_SIZE"])
if len(backup) != 524288:
    raise gdb.GdbError(f"STM32_BACKUP_SIZE_INVALID: {len(backup)}")
if len(before) != 262144 or len(after) != 262144:
    raise gdb.GdbError(f"STM32_JOURNAL_SIZE_INVALID: {len(before)}/{len(after)}")
if len(readback) != expected:
    raise gdb.GdbError(f"STM32_APP_READBACK_SIZE_INVALID: {len(readback)}")
if backup[0x40000:] != before:
    raise gdb.GdbError("STM32_BACKUP_JOURNAL_SLICE_MISMATCH")
if image != readback:
    raise gdb.GdbError("STM32_APP_VERIFY_FAILED")
if before != after:
    raise gdb.GdbError("STM32_JOURNAL_CHANGED_BEFORE_BOOT")
print("STM32_HALTED_VERIFY=PASS; JOURNAL=PRESERVED")
trigger = Path(os.environ["NOSTOS_BOOT_TRIGGER"])
ack = Path(os.environ["NOSTOS_BOOT_ACK"])
trigger.touch(exist_ok=False)
ack_deadline = time.monotonic() + 5.0
while time.monotonic() < ack_deadline and not ack.exists():
    time.sleep(0.05)
if not ack.exists():
    raise gdb.GdbError("STM32_BOOT_CAPTURE_ARM_TIMEOUT")
print("STM32_BOOT_CAPTURE_ARMED=PASS")
end
monitor reset
detach
quit
GDBPY
} >"$GDB_COMMANDS"

cat >"$BOOT_CAPTURE" <<'PY'
import os
from pathlib import Path
import select
import sys
import termios
import time

port = os.environ["NOSTOS_VCP_CALLOUT"]
marker = b"NOSTOS_V2_BOOT=READY source=2 session=1"
trigger = Path(os.environ["NOSTOS_BOOT_TRIGGER"])
ack = Path(os.environ["NOSTOS_BOOT_ACK"])
fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
try:
    settings = termios.tcgetattr(fd)
    settings[0] = 0
    settings[1] = 0
    settings[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    settings[3] = 0
    settings[4] = termios.B115200
    settings[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, settings)

    prearm_deadline = time.monotonic() + 90.0
    while not trigger.exists():
        if time.monotonic() >= prearm_deadline:
            print("STM32_BOOT_CAPTURE_PREARM_TIMEOUT", file=sys.stderr)
            raise SystemExit(2)
        ready, _, _ = select.select([fd], [], [], 0.2)
        if ready:
            try:
                os.read(fd, 4096)
            except BlockingIOError:
                pass

    # ACK only after old UART bytes have been discarded and capture is armed.
    termios.tcflush(fd, termios.TCIFLUSH)
    observed = bytearray()
    ack.touch(exist_ok=False)
    boot_deadline = time.monotonic() + 45.0
    while time.monotonic() < boot_deadline and marker not in observed:
        ready, _, _ = select.select([fd], [], [], 0.2)
        if ready:
            try:
                data = os.read(fd, 4096)
                observed.extend(data)
            except BlockingIOError:
                pass
    os.write(1, observed)
    raise SystemExit(0 if marker in observed else 1)
finally:
    os.close(fd)
PY

printf 'STM32_HALTED_INSTALL_BEGIN serial=%s bytes=%s\n' "$EXPECTED_SERIAL" "$image_size"
(
    cd -- "$OUTPUT_DIR"
    PYTHONDONTWRITEBYTECODE=1 "$PYTHON" "$BOOT_CAPTURE_BASENAME"
) >"$BOOT_LOG" 2>&1 &
CAPTURE_PID=$!
sleep 0.1
kill -0 "$CAPTURE_PID" 2>/dev/null || {
    printf 'STM32_BOOT_CAPTURE_FAILED_TO_START\n' >&2; exit 70;
}
"$ST_UTIL" --serial "$EXPECTED_SERIAL" --connect-under-reset --listen_port "$GDB_PORT" \
    >"$STUTIL_LOG" 2>&1 &
STUTIL_PID=$!
for _ in {1..50}; do
    kill -0 "$STUTIL_PID" 2>/dev/null || {
        printf 'STM32_GDB_SERVER_FAILED\n' >&2; exit 70;
    }
    lsof -nP -iTCP:"$GDB_PORT" -sTCP:LISTEN >/dev/null 2>&1 && break
    sleep 0.1
done
lsof -nP -iTCP:"$GDB_PORT" -sTCP:LISTEN >/dev/null 2>&1 || {
    printf 'STM32_GDB_SERVER_TIMEOUT\n' >&2; exit 70;
}
FLASH_PHASE="debugger"
(
    cd -- "$OUTPUT_DIR"
    "$GDB" --batch --quiet -x "$GDB_COMMANDS_BASENAME" "$ELF"
) >"$GDB_LOG" 2>&1 || {
    printf 'STM32_HALTED_INSTALL_FAILED: see %s\n' "$GDB_LOG" >&2; exit 74;
}
stop_stutil
FLASH_PHASE="boot_wait"
if ! wait "$CAPTURE_PID"; then
    CAPTURE_PID=""
    printf 'STM32_BOOT_MARKER_NOT_OBSERVED\n' >&2
    exit 74
fi
CAPTURE_PID=""
grep -a -F 'NOSTOS_V2_BOOT=READY source=2 session=1' "$BOOT_LOG" >/dev/null || {
    printf 'STM32_BOOT_MARKER_NOT_OBSERVED\n' >&2; exit 74;
}
FLASH_PHASE="booted"

image_sha="$(shasum -a 256 "$IMAGE" | awk '{print $1}')"
backup_sha="$(shasum -a 256 "$BACKUP" | awk '{print $1}')"
journal_sha="$(shasum -a 256 "$JOURNAL_AFTER" | awk '{print $1}')"
{
    printf 'RESULT=PASS\n'
    printf 'serial=%s\n' "$EXPECTED_SERIAL"
    printf 'app_bytes=%s\n' "$image_size"
    printf 'app_sha256=%s\n' "$image_sha"
    printf 'backup_sha256=%s\n' "$backup_sha"
    printf 'journal_sha256=%s\n' "$journal_sha"
    printf 'journal_preserved_before_boot=1\n'
    printf 'mass_erase=0\n'
    printf 'runtime_ready_marker=PASS\n'
} >"$MANIFEST"
printf 'STM32_FLASH=PASS serial=%s app_sha256=%s JOURNAL=PRESERVED BOOT_MARKER=PASS\n' \
    "$EXPECTED_SERIAL" "$image_sha"
