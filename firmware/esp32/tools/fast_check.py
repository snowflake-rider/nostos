#!/usr/bin/env python3
"""Read-only Layer8 observer. PASS_OBSERVED is host-log correlation, not ACK proof."""
import argparse
from collections import Counter
from contextlib import ExitStack
import json
import math
import re
import select
import sys
import time

IDENTITIES = {
    "STM32": "066DFF485277504867161930",
    "D6": "14:C1:9F:CE:F0:D4",
    "76": "14:C1:9F:CE:EC:74",
    "B6": "44:1B:F6:FF:BA:B4",
}
ANSI = re.compile(r"\x1b\[[0-9;]*m")
FIELDS = re.compile(r"(\w+)=([^\s;]+)")
STAGES = re.compile(r"\b(UART_RX|MESH_TX|MESH_RX|UART_TX)\b.*\bid=0x([0-9a-fA-F]+)")
COUNTERS = ("valid", "noop", "invalid", "hw_errors")


class Observer:
    """Feed timestamped USB input; tick returns a request for a fresh snapshot."""

    def __init__(self, emit, source="D6", peers=("76", "B6"), window=3.0):
        self.emit, self.source, self.peers, self.window = emit, source, peers, window
        self.boards = (source, *peers)
        self.status, self.stats = {}, {}
        self.trial = None
        self.number = 0
        self.ready_announced = False
        self.last_stm = 0.0
        self.idle_reported = False
        self.last_warning = {}
        self.stm_text = bytearray()

    def ready(self, now):
        addresses = []
        for board in self.boards:
            if board not in self.status or board not in self.stats:
                return False
            fields, timestamp = self.status[board]
            if now - timestamp > 2.5 or now - self.stats[board][1] > 2.5:
                return False
            if (fields.get("event_ready") != "1" or fields.get("sub_C001") != "1"
                    or fields.get("pub", "").lower() != "0xc001"):
                return False
            try:
                address = int(fields["primary"], 16)
            except (KeyError, ValueError):
                return False
            if not 0 < address < 0x8000:
                return False
            addresses.append(address)
        return len(set(addresses)) == len(addresses)

    def warning(self, reason, now):
        if self.trial is not None:
            self.trial["issues"].add(reason)
        if now - self.last_warning.get(reason, -100) >= 2:
            self.emit({"kind": "warning", "reason": reason})
            self.last_warning[reason] = now

    def stm(self, data, now):
        for byte in data:
            if byte == 0x13:
                self._stm_event(now)
                continue
            if byte == 0x0D:
                continue
            if byte == 0x0A:
                line = self.stm_text.decode("ascii", errors="replace")
                self.stm_text.clear()
                if line and not line.startswith(("STAU ", "STATUS audio=")):
                    self.warning("STM32_UNEXPECTED_TEXT", now)
                continue
            if 0x20 <= byte <= 0x7E and len(self.stm_text) < 256:
                self.stm_text.append(byte)
                continue
            self.stm_text.clear()
            self.warning(f"STM32_UNEXPECTED_BYTE_0x{byte:02x}", now)

    def _stm_event(self, now):
        self.last_stm, self.idle_reported = now, False
        if self.trial is None:
            self.number += 1
            self.trial = {
                "number": self.number, "start": now, "deadline": now + self.window,
                "counts": Counter({"STM32": 0, f"{self.source}.UART_RX": 0,
                                   f"{self.source}.MESH_TX": 0,
                                   **{f"{p}.MESH_RX": 0 for p in self.peers}}),
                "issues": set(), "baseline": {b: dict(s[0]) for b, s in self.stats.items()},
                "source_address": self.status.get(self.source, ({}, 0))[0].get("primary"),
                "requested": False,
            }
            if not self.ready(now):
                self.trial["issues"].add("NOT_READY_OR_STALE_BASELINE")
            self.emit({"kind": "start", "number": self.number, "window": self.window})
        elif now >= self.trial["deadline"]:
            self.warning("PRESS_DURING_END_SNAPSHOT_REPEAT_TEST", now)
            return
        self.trial["counts"]["STM32"] += 1
        self.emit({"kind": "stage", "stage": "STM32", "count": self.trial["counts"]["STM32"]})

    def line(self, board, raw, now):
        if board not in self.boards:
            return
        line = ANSI.sub("", raw)
        fields = dict(FIELDS.findall(line))
        if "BOOT_START" in line or "UART1_READY" in line:
            self.status.pop(board, None)
            self.stats.pop(board, None)
            self.ready_announced = False
            self.warning(f"{board}_RESTARTED", now)
        if "STATUS name=" in line:
            previous = self.status.get(board)
            config_keys = ("primary", "net", "app", "pub", "sub_C001", "event_ready")
            if self.trial and previous and any(fields.get(k) != previous[0].get(k) for k in config_keys):
                self.warning(f"{board}_CONFIG_CHANGED", now)
            self.status[board] = fields, now
            if fields.get("event_ready") != "1":
                self.warning(f"{board}_MESH_NOT_READY", now)
        if "QUEUE pending=" in line and "uart_rx valid=" in line:
            try:
                counters = {key: int(fields[key]) for key in COUNTERS}
            except (KeyError, ValueError):
                self.warning(f"{board}_BAD_STATUS", now)
                return
            previous = self.stats.get(board)
            if previous:
                delta = {key: counters[key] - previous[0][key] for key in COUNTERS}
                if any(value < 0 for value in delta.values()):
                    self.warning(f"{board}_COUNTERS_RESET", now)
                if any(delta[key] > 0 for key in ("noop", "invalid", "hw_errors")):
                    self.warning(f"{board}_UART_NOISE", now)
            self.stats[board] = counters, now
        match = STAGES.search(line)
        if not match:
            return
        stage, ident = match.group(1), int(match.group(2), 16)
        if self.trial is None:
            self.warning(f"UNMATCHED_{board}_{stage}_0x{ident:02x}", now)
            return
        if now >= self.trial["deadline"]:
            self.warning("LATE_OR_EXTRA_EVENT", now)
            return
        if ident != 0x13:
            self.warning(f"UNEXPECTED_{board}_{stage}_0x{ident:02x}", now)
            return
        expected_source = self.trial["source_address"]
        if stage in ("MESH_RX", "MESH_TX", "UART_TX") and fields.get("source") != expected_source:
            self.warning(f"{board}_WRONG_SOURCE", now)
            return
        accepted = fields.get("result") == "queued" if stage.endswith("RX") else fields.get("api") == "accepted"
        if not accepted:
            self.warning(f"{board}_{stage}_NOT_ACCEPTED", now)
            return
        key = f"{board}.{stage}"
        if key in self.trial["counts"]:
            self.trial["counts"][key] += 1
            self.emit({"kind": "stage", "stage": key, "count": self.trial["counts"][key]})
        elif stage != "UART_TX":
            self.warning(f"UNEXPECTED_DIRECTION_{key}", now)

    def tick(self, now):
        if self.trial is None:
            ready = self.ready(now)
            if ready and not self.ready_announced:
                self.emit({"kind": "ready", "source": self.source, "peers": list(self.peers)})
            self.ready_announced = ready
            if now - self.last_stm >= 10 and not self.idle_reported:
                self.emit({"kind": "idle", "reason": "NO_STM32_BYTE_YET"})
                self.idle_reported = True
            return False
        trial = self.trial
        if now < trial["deadline"]:
            return False
        if not trial["requested"]:
            trial["requested"] = True
            return True
        fresh = all(b in self.stats and self.stats[b][1] >= trial["deadline"]
                    and b in self.status and self.status[b][1] >= trial["deadline"] for b in self.boards)
        if not fresh and now < trial["deadline"] + 1:
            return False
        self.finish(now, None if fresh else "END_STATUS_MISSING")
        return False

    def finish(self, now, reason=None):
        if self.trial is None:
            return
        trial = self.trial
        issues = trial["issues"]
        if reason:
            issues.add(reason)
        if not self.ready(now):
            issues.add("NOT_READY_OR_STALE_STATUS")
        expected = trial["counts"]["STM32"]
        delta = {}
        for board in self.boards:
            if board not in trial["baseline"] or board not in self.stats:
                issues.add(f"{board}_COUNTERS_MISSING")
                continue
            delta[board] = {key: self.stats[board][0][key] - trial["baseline"][board][key] for key in COUNTERS}
            if any(value < 0 for value in delta[board].values()):
                issues.add(f"{board}_COUNTERS_RESET")
            if any(delta[board][key] for key in ("noop", "invalid", "hw_errors")):
                issues.add(f"{board}_UART_NOISE")
            if delta[board]["valid"] != (expected if board == self.source else 0):
                issues.add(f"{board}_UART_COUNT_MISMATCH")
        missing = [key for key, count in trial["counts"].items() if count < expected]
        if any(count > expected for count in trial["counts"].values()):
            issues.add("DUPLICATE_OR_EXTRA_EVENT")
        verdict = "WARN" if issues else ("INCOMPLETE" if missing else "PASS_OBSERVED")
        self.emit({"kind": "result", "number": trial["number"], "verdict": verdict,
                   "counts": dict(trial["counts"]), "missing": missing,
                   "issues": sorted(issues), "uart_delta": delta})
        self.trial = None
        self.ready_announced = False


def printer(as_json):
    def emit(record):
        if as_json:
            print(json.dumps(record, ensure_ascii=False), flush=True)
            return
        kind = record["kind"]
        if kind == "ready":
            print(f"READY — 버튼을 누르세요. STM32 → {record['source']} → {', '.join(record['peers'])}", flush=True)
        elif kind == "stage":
            print(f"  ✓ {record['stage']} 0x13 × {record['count']}", flush=True)
        elif kind == "result":
            print(f"[{record['verdict']}] #{record['number']} {record['counts']}", flush=True)
            if record["missing"]:
                print("  미수신: " + ", ".join(record["missing"]), flush=True)
            if record["issues"]:
                print("  확인 필요: " + ", ".join(record["issues"]), flush=True)
            print("  UART 카운터 증가: " + str(record["uart_delta"]), flush=True)
        elif kind == "start":
            print(f"TEST #{record['number']} — {record['window']:g}초 판정 창 시작", flush=True)
        elif kind == "idle":
            print("WAIT — STM32 송신 없음. 누른 상태라면 D10/버튼/GND부터 확인하세요.", flush=True)
        elif kind == "warning":
            print("WARN — " + record["reason"], flush=True)
        else:
            print(kind.upper() + " — " + str({k: v for k, v in record.items() if k != "kind"}), flush=True)
    return emit


def run_replay(observer, filename):
    with ExitStack() as stack:
        stream = sys.stdin if filename == "-" else stack.enter_context(open(filename, encoding="utf-8"))
        last = 0.0
        for raw in stream:
            record = json.loads(raw)
            now = float(record["at"])
            if not math.isfinite(now) or now < last:
                raise ValueError("replay timestamps must be finite and nondecreasing")
            last = now
            if "hex" in record:
                if record.get("board") != "STM32":
                    raise ValueError("hex input is reserved for STM32")
                observer.stm(bytes.fromhex(record["hex"]), now)
            elif "stm_text" in record:
                if record.get("board") != "STM32":
                    raise ValueError("stm_text input is reserved for STM32")
                observer.stm(record["stm_text"].encode("ascii"), now)
            elif "line" in record:
                observer.line(record["board"], record["line"], now)
            observer.tick(now)
        observer.tick(last + observer.window + 2)
        observer.finish(last + observer.window + 2, "REPLAY_ENDED")


def run_live(observer, duration):
    import serial
    from serial.tools import list_ports

    class NoControlSerial(serial.Serial):
        # Native USB port open must NOT toggle reset/boot control lines.
        def _update_dtr_state(self):
            pass

        def _update_rts_state(self):
            pass

    found = {}
    for port in list_ports.comports():
        key = (port.serial_number or "").upper()
        if not key:
            continue
        if key in found:
            raise ValueError(f"duplicate USB identity: {key}")
        found[key] = port.device
    required = ("STM32", *observer.boards)
    missing = [b for b in required if IDENTITIES[b] not in found]
    if missing:
        raise ValueError("USB 누락: " + ", ".join(missing))
    devices, buffers = {}, {}
    start = time.monotonic()
    with ExitStack() as stack:
        for board in required:
            port = found[IDENTITIES[board]]
            device = stack.enter_context(NoControlSerial(port, 115200, timeout=0, write_timeout=.2, exclusive=True))
            device.reset_input_buffer()
            devices[device], buffers[device] = board, bytearray()
            observer.emit({"kind": "connected", "board": board, "port": port})
        observer.emit({"kind": "notice", "text": "status 명령만 사용. 리셋/플래시/키 변경 없음. Ctrl-C 종료."})
        next_status = 0.0
        try:
            while not duration or time.monotonic() - start < duration:
                now = time.monotonic() - start
                if now >= next_status:
                    for device, board in devices.items():
                        if board != "STM32":
                            device.write(b"status\n")
                    next_status = now + .5
                readable, _, _ = select.select(list(devices), [], [], .02)
                # Drain STM32 first when several USB interfaces are readable together.
                readable.sort(key=lambda dev: devices[dev] != "STM32")
                for device in readable:
                    data = device.read(16384)
                    if not data:
                        raise OSError(f"USB disconnected: {devices[device]}")
                    now = time.monotonic() - start
                    if devices[device] == "STM32":
                        observer.stm(data, now)
                        continue
                    buffers[device].extend(data)
                    while b"\n" in buffers[device]:
                        line, _, remainder = buffers[device].partition(b"\n")
                        buffers[device] = remainder
                        observer.line(devices[device], line.decode("utf-8", errors="replace"), now)
                    if len(buffers[device]) > 16384:
                        buffers[device].clear()
                        observer.warning(f"{devices[device]}_LOG_OVERFLOW", now)
                if observer.tick(time.monotonic() - start):
                    next_status = 0.0
        finally:
            observer.finish(time.monotonic() - start, "MONITOR_STOPPED")
    observer.emit({"kind": "closed", "text": "모든 포트 해제"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", choices=("D6", "76", "B6"), default="D6")
    parser.add_argument("--window", type=float, default=3.0, help="판정 창 초 (기본 3)")
    parser.add_argument("--duration", type=float, default=0, help="0이면 Ctrl-C까지 유지")
    parser.add_argument("--replay", metavar="JSONL", help="가짜 로그 재생; - 는 stdin, USB 접근 없음")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if not math.isfinite(args.window) or args.window <= 0 or not math.isfinite(args.duration) or args.duration < 0:
        parser.error("window must be positive; duration must be nonnegative and finite")
    emit = printer(args.json)
    peers = tuple(b for b in ("76", "B6", "D6") if b != args.source)
    observer = Observer(emit, args.source, peers, args.window)
    try:
        if args.replay is not None:
            run_replay(observer, args.replay)
        else:
            run_live(observer, args.duration)
    except KeyboardInterrupt:
        emit({"kind": "closed", "text": "Ctrl-C; 포트 해제"})
        return 0
    except (OSError, ValueError, ImportError) as error:
        emit({"kind": "error", "message": str(error)})
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
