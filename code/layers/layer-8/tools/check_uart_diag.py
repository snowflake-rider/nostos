#!/usr/bin/env python3
"""Read-only status integration check. DIAG_PRESENT is not UART delivery proof."""
import argparse
import time

import serial
from serial.tools import list_ports

from fast_check import ANSI, IDENTITIES


class NoControlSerial(serial.Serial):
    def _update_dtr_state(self):
        pass

    def _update_rts_state(self):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", choices=("D6", "76", "B6"), required=True)
    args = parser.parse_args()
    ports = [p.device for p in list_ports.comports()
             if (p.serial_number or "").upper() == IDENTITIES[args.board]]
    if len(ports) != 1:
        raise SystemExit(f"USB identity missing or ambiguous: {args.board}")
    found = set()
    markers = ("UART_DIAG ", "UART_DIAG_RX ", "UART_DIAG_TX ", "UART_DIAG_PIN ")
    with NoControlSerial(ports[0], 115200, timeout=.1, write_timeout=.5,
                         exclusive=True) as port:
        port.write(b"status\n")
        deadline = time.monotonic() + 2
        pending = bytearray()
        while time.monotonic() < deadline:
            pending.extend(port.read(4096))
            while b"\n" in pending:
                line, _, pending = pending.partition(b"\n")
                text = ANSI.sub("", line.decode(errors="replace")).strip()
                if "UART_DIAG" in text or "STATUS name=" in text:
                    print(args.board, text, flush=True)
                if "UART_DIAG_ERROR" in text:
                    raise SystemExit("FAIL: diagnostic hardware read failed")
                found.update(m for m in markers if m in text)
            if len(found) == len(markers):
                print("DIAG_PRESENT: read-only status available; not a link PASS")
                return
    raise SystemExit("FAIL: missing diagnostic status: " + ", ".join(set(markers) - found))


if __name__ == "__main__":
    main()
