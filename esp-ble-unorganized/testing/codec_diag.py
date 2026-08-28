#!/usr/bin/env python3
"""Bounded VS1003B observation/commands over ST-LINK USB, without board reset.

Requires pyserial and BUTTON_OUTPUT_TEST firmware. Evidence never means audible PASS.
"""
import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import select
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "layers/layer-8/tools"))
from check_uart_diag import NoControlSerial
from fast_check import ANSI, IDENTITIES
from serial.tools import list_ports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--command", choices=("observe", "registers", "reset", "sdi", "sine"),
                        default="observe")
    parser.add_argument("--seconds", type=float, default=15)
    parser.add_argument("--mesh", action="store_true", help="Also observe D6 and 76; query status only")
    parser.add_argument("--confirm-tone", action="store_true", help="Ready for a one-second test tone")
    parser.add_argument("--out", type=Path, help="New evidence directory; must not already exist")
    args = parser.parse_args()
    if not 3 <= args.seconds <= 3600:
        parser.error("--seconds must be between 3 and 3600")
    if args.command == "sine" and not args.confirm_tone:
        parser.error("sine needs --confirm-tone after the listener is ready")
    if args.out:
        run = args.out.resolve()
        run.mkdir(parents=True, exist_ok=False)
    else:
        results = ROOT / "testing/results"
        results.mkdir(exist_ok=True)
        run = Path(tempfile.mkdtemp(prefix="codec-", dir=results))
    print(f"EVIDENCE {run}", flush=True)
    commands = {"registers": b"d", "reset": b"r", "sdi": b"t", "sine": b"s"}
    started = time.monotonic()
    sent = args.command == "observe"
    last_status = {}
    records = []
    with ExitStack() as stack:
        raw = stack.enter_context((run / "raw.jsonl").open("x"))
        console = stack.enter_context((run / "console.log").open("x"))

        def record(board, line):
            item = {"at": round(time.monotonic() - started, 6), "board": board, "line": line}
            records.append(item)
            raw.write(json.dumps(item) + "\n")
            raw.flush()
            show = board in ("STM32", "HOST") or any(token in line for token in
                ("UART_RX id=", "MESH_TX id=", "MESH_RX source=0x0005", "UART_DIAG_ERROR"))
            if board == "STM32" and line.startswith("STATUS "):
                show = last_status.get(board) != line
                last_status[board] = line
            if show:
                output = f"{item['at']:8.3f}s {board} {line}"
                print(output, flush=True)
                console.write(output + "\n")
                console.flush()

        ports = list(list_ports.comports())
        devices = {}
        for board in (("STM32", "D6", "76") if args.mesh else ("STM32",)):
            matches = [p.device for p in ports if (p.serial_number or "").upper() == IDENTITIES[board]]
            if len(matches) != 1:
                raise RuntimeError(f"USB identity missing or ambiguous: {board}")
            device = stack.enter_context(NoControlSerial(matches[0], 115200, timeout=0,
                                                         write_timeout=.5, exclusive=True))
            devices[device] = board
            record("HOST", f"CONNECTED {board} {matches[0]}")
        stm = next(d for d, b in devices.items() if b == "STM32")
        pending = {d: bytearray() for d in devices}
        next_status = 0.0
        # Only a read-register command first: require a diagnostic-firmware response
        # before a reset/test command. No bytes are sent to USART1 by this script.
        if args.command != "observe":
            stm.write(b"d")
        record("HOST", "OBSERVER_READY")
        while time.monotonic() - started < args.seconds and not (run / "stop").exists():
            elapsed = time.monotonic() - started
            if elapsed >= next_status:
                for device, board in devices.items():
                    if board != "STM32":
                        device.write(b"status\n")
                next_status = elapsed + 2
            if not sent and elapsed > 3:
                raise RuntimeError("No diagnostic-firmware handshake; no test/reset command sent")
            for device in select.select(list(devices), [], [], .1)[0]:
                data = device.read(16384)
                if not data:
                    raise RuntimeError(f"Disconnected: {devices[device]}")
                pending[device].extend(data)
                if len(pending[device]) > 65536:
                    raise RuntimeError("Serial line exceeded 64 KiB")
                while b"\n" in pending[device]:
                    line, _, pending[device] = pending[device].partition(b"\n")
                    line = ANSI.sub("", line.decode(errors="replace")).strip()
                    board = devices[device]
                    record(board, line)
                    if not sent and board == "STM32" and line.startswith("CODEC phase=manual "):
                        if args.command != "registers":
                            stm.write(commands[args.command])
                        sent = True
                        record("HOST", f"COMMAND_SENT {args.command}")
        record("HOST", "PORTS_CLOSED_ON_EXIT")
    (run / "summary.json").write_text(json.dumps({
        "command": args.command, "command_sent": sent,
        "physical_audio": "NOT_ASSESSED", "records": len(records),
        "diagnostics": [x for x in records if x["board"] == "STM32" and
                        x["line"].startswith(("CODEC", "SDI_TEST", "SINE", "AUDIO_"))],
    }, indent=2) + "\n")
    print("PHYSICAL_AUDIO=NOT_ASSESSED (listener report required)")


if __name__ == "__main__":
    main()
