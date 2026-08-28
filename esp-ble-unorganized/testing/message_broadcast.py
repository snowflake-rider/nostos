#!/usr/bin/env python3
"""Observe 8 event IDs through real STM32 USART1 and Layer 8 group delivery.

Default is readiness only. --send explicitly enables finite test transmissions.
No flash, reset, key changes, synthetic UART injection on ESP32, or auto retries.
"""
import argparse
from collections import Counter
from contextlib import ExitStack
import json
import math
from pathlib import Path
import re
import select
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "layers/layer-8/tools"))
from fast_check import ANSI, FIELDS, IDENTITIES

MESSAGES = {
    0x10: "감속 요청", 0x11: "가속 요청", 0x12: "안전 알림", 0x13: "정지 요청",
    0x20: "후방 안전", 0x21: "후방 경고", 0x30: "낙차 감지", 0x31: "SOS",
}
CONFIG = ("name", "primary", "net", "app", "pub", "sub_C001", "event_ready",
          "ttl", "period", "retransmit", "relay")
SECTIONS = {
    "config": "STATUS name=", "uart": "QUEUE pending=", "rx": "MESH_RX valid=",
    "tx": "MESH_TX accepted=", "uart_tx": "UART_TX accepted=",
    "stack": "MESH_STACK complete_ok=", "diag": "UART_DIAG version=",
}
STAGE = re.compile(r"\b(UART_RX|MESH_TX|MESH_RX|UART_TX)\b.*\bid=0x([0-9a-fA-F]+)")


def collect_snapshots(records, boards):
    snapshots = {board: {} for board in boards}
    for item in records:
        if item["board"] not in snapshots:
            continue
        for section, marker in SECTIONS.items():
            if marker in item["line"]:
                snapshots[item["board"]][section] = dict(FIELDS.findall(item["line"]))
    return snapshots


def readiness(snapshots):
    issues, addresses, keys = [], [], []
    for board, sections in snapshots.items():
        if set(SECTIONS) - sections.keys():
            issues.append(f"{board}: missing status sections")
            continue
        cfg = sections["config"]
        try:
            address = int(cfg["primary"], 16)
            if not 0 < address < 0x8000:
                raise ValueError("address")
            addresses.append(address)
            keys.append((int(cfg["net"], 16), int(cfg["app"], 16)))
            if any(value == 0xFFFF for value in keys[-1]):
                raise ValueError("unused key")
            if any(cfg.get(k) != v for k, v in
                   {"event_ready": "1", "sub_C001": "1", "relay": "0",
                    "ttl": "7", "period": "0", "retransmit": "0"}.items()):
                raise ValueError("not ready / relay enabled / wrong publication settings")
            if int(cfg["pub"], 16) != 0xC001:
                raise ValueError("wrong group")
            if int(sections["uart"]["pending"]) or int(sections["diag"]["buffered"]):
                raise ValueError("pending queue or UART bytes")
            diag = sections["diag"]
            if (diag["port"], diag["data"], diag["parity"], diag["stop"], diag["flow"]) != (
                    "1", "8", "none", "1", "0") or abs(int(diag["baud"]) - 115200) > 1200:
                raise ValueError("UART settings")
        except (KeyError, ValueError) as exc:
            issues.append(f"{board}: {exc}")
    if len(addresses) != len(set(addresses)):
        issues.append("duplicate Mesh address")
    if len(set(keys)) > 1:
        issues.append("NetKey/AppKey indices differ (matching indices alone do not prove equal keys)")
    return issues


def evaluate(ident, records, before, after, source, peers, expected_seq):
    """Judge one finite trial. Inputs are real log records or explicit test fixtures."""
    counts = Counter({"STM32.TX": 0, f"{source}.UART_RX": 0, f"{source}.MESH_TX": 0,
                      **{f"{p}.MESH_RX": 0 for p in peers}})
    issues = readiness(before) + readiness(after)
    try:
        address = int(before[source]["config"]["primary"], 16)
    except (KeyError, ValueError):
        address = -1
    for item in records:
        board, line = item["board"], item["line"]
        if board not in ("STM32", source, *peers):
            continue
        if any(marker in line for marker in ("OUTPUT_TEST_READY", "UART1_READY", "APP_STARTED", "BOOT_START")):
            issues.append(f"{board}: restarted")
        fields = dict(FIELDS.findall(line))
        if board == "STM32":
            if "BUTTON " in line or "MESSAGE_TEST_REJECT" in line:
                issues.append("STM32: manual input or rejected command during test")
            if "MESSAGE_TEST_TX " in line:
                try:
                    ok = (int(fields["id"], 16) == ident and fields["uart"] == "OK"
                          and int(fields["seq"]) == expected_seq)
                except (ValueError, KeyError):
                    ok = False
                if ok:
                    counts["STM32.TX"] += 1
                else:
                    issues.append("STM32: wrong ID/sequence or HAL failure")
            continue
        match = STAGE.search(line)
        if not match:
            continue
        stage, received = match[1], int(match[2], 16)
        key = f"{board}.{stage}"
        try:
            origin_ok = stage == "UART_RX" or int(fields["source"], 16) == address
        except (ValueError, KeyError):
            origin_ok = False
        accepted = fields.get("result") == "queued" if stage.endswith("RX") else fields.get("api") == "accepted"
        if received != ident or not origin_ok or not accepted:
            issues.append(f"{key}: unexpected ID/source/result")
        elif key in counts:
            counts[key] += 1
        elif not (board in peers and stage == "UART_TX"):
            issues.append(f"{key}: unexpected direction")
    for key, count in counts.items():
        if count != 1:
            issues.append(f"{key}: expected 1, observed {count}")
    deltas = {}
    for board in (source, *peers):
        try:
            old, new = before[board], after[board]
            if any(old["config"].get(k) != new["config"].get(k) for k in CONFIG):
                issues.append(f"{board}: configuration changed")
            deltas[board] = {}
            for section, fields in {
                "uart": ("valid", "noop", "invalid", "hw_errors"),
                "rx": ("valid", "invalid", "self", "not_ready"),
                "tx": ("accepted", "failed", "full", "expired"),
                "uart_tx": ("accepted", "failed", "full", "expired"),
                "stack": ("complete_ok", "failed"),
            }.items():
                for field in fields:
                    delta = int(new[section][field]) - int(old[section][field])
                    deltas[board][f"{section}.{field}"] = delta
                    if delta < 0 or (field in ("noop", "invalid", "hw_errors", "not_ready", "failed", "full", "expired") and delta):
                        issues.append(f"{board}: {section}.{field} delta={delta}")
            if deltas[board]["uart.valid"] != (1 if board == source else 0):
                issues.append(f"{board}: UART input count mismatch")
            if deltas[board]["tx.accepted"] != (1 if board == source else 0):
                issues.append(f"{board}: Mesh transmit count mismatch")
            if board in peers and deltas[board]["rx.valid"] - deltas[board]["rx.self"] != 1:
                issues.append(f"{board}: Mesh receive count mismatch")
        except (KeyError, ValueError):
            issues.append(f"{board}: malformed/missing counters")
    return {"id": f"0x{ident:02X}", "name": MESSAGES[ident], "sequence": expected_seq,
            "verdict": "PASS_OBSERVED" if not issues else "FAIL",
            "counts": dict(counts), "issues": sorted(set(issues)), "deltas": deltas}


class Capture:
    def __init__(self, stack, run, boards):
        from check_uart_diag import NoControlSerial
        from serial.tools import list_ports
        self.started = time.monotonic()
        self.records, self.devices, self.pending = [], {}, {}
        self.run = run
        self.raw = stack.enter_context((run / "raw.jsonl").open("x"))
        self.console = stack.enter_context((run / "console.log").open("x"))
        ports = list(list_ports.comports())
        # Resolve every required board before opening any port. Never shrink the peer set.
        resolved = {}
        for board in ("STM32", *boards):
            matches = [p for p in ports if (p.serial_number or "").upper() == IDENTITIES[board]]
            if len(matches) != 1:
                raise RuntimeError(f"USB identity missing/ambiguous: {board}")
            resolved[board] = matches[0].device
        (run / "devices.json").write_text(json.dumps(resolved, indent=2) + "\n")
        for board, path in resolved.items():
            self.devices[board] = stack.enter_context(NoControlSerial(
                path, 115200, timeout=0, write_timeout=.5, exclusive=True))
            self.pending[board] = bytearray()
            self.record("HOST", f"CONNECTED {board} {path}")

    def record(self, board, line):
        item = {"at": round(time.monotonic() - self.started, 6), "board": board, "line": line}
        self.records.append(item)
        self.raw.write(json.dumps(item, ensure_ascii=False) + "\n")
        self.raw.flush()
        self.console.write(f"{item['at']:8.3f}s {board} {line}\n")
        self.console.flush()
        if board == "HOST" or "MESSAGE_TEST_" in line or STAGE.search(line):
            print(f"{item['at']:8.3f}s {board} {line}", flush=True)

    def send(self, board, data):
        if self.devices[board].write(data) != len(data):
            raise RuntimeError(f"{board}: incomplete USB write; no retry")
        self.record("HOST", f"USB_WRITE board={board} hex={data.hex()}")

    def pump(self, seconds):
        deadline = time.monotonic() + seconds
        reverse = {device: board for board, device in self.devices.items()}
        while time.monotonic() < deadline:
            if (self.run / "stop").exists():
                raise RuntimeError("stop file requested")
            for device in select.select(list(reverse), [], [], min(.1, max(0, deadline-time.monotonic())))[0]:
                board = reverse[device]
                data = device.read(16384)
                if not data:
                    raise RuntimeError(f"{board}: disconnected")
                self.pending[board].extend(data)
                if len(self.pending[board]) > 65536:
                    raise RuntimeError(f"{board}: serial line exceeded 64 KiB")
                while b"\n" in self.pending[board]:
                    raw, _, self.pending[board] = self.pending[board].partition(b"\n")
                    self.record(board, ANSI.sub("", raw.decode(errors="replace")).strip())

    def wait_line(self, board, marker, start, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for item in self.records[start:]:
                if item["board"] == board and item["line"].startswith(marker):
                    return dict(FIELDS.findall(item["line"]))
            self.pump(.02)
        raise RuntimeError(f"{board}: no {marker}; no automatic retry")

    def snapshot(self, boards):
        start = len(self.records)
        for board in boards:
            self.send(board, b"status\n")
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            self.pump(.15)
            result = collect_snapshots(self.records[start:], boards)
            if all(set(SECTIONS) <= sections.keys() for sections in result.values()):
                return result
        raise RuntimeError("Fresh status response missing")


def write_report(run, summary):
    (run / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    rows = ["# 메시지별 Group Delivery 시험", "", f"결과: **{summary['verdict']}**", "",
            "| ID | 메시지 | 회차 | STM32 TX | 송신 UART RX | 송신 Mesh TX | 수신 노드별 Mesh RX | 판정 |",
            "| --- | --- | ---: | ---: | ---: | ---: | --- | --- |"]
    for case in summary["cases"]:
        counts = case["counts"]
        source = summary["source"]
        peers = ", ".join(f"{p}={counts[p+'.MESH_RX']}" for p in summary["peers"])
        rows.append(f"| {case['id']} | {case['name']} | {case['repeat']} | {counts['STM32.TX']} | "
                    f"{counts[source+'.UART_RX']} | {counts[source+'.MESH_TX']} | {peers} | {case['verdict']} |")
    rows += ["", f"실행한 시험: {len(summary['cases'])}/{summary['planned_cases']}", "",
             "센서/물리 버튼, 상대 STM32 수신·출력, 다중 홉 Relay, 무손실 보장은 이 시험의 판정 범위가 아니다.",
             "PASS_OBSERVED는 실제 USB 로그의 ID/source/건수와 전후 카운터가 일치했다는 뜻이다.",
             "앱 payload에 고유 sequence가 없으므로 개별 무선 패킷의 인과관계/ACK 증명은 아니다."]
    if summary.get("error"):
        rows += ["", "중단 이유: " + summary["error"]]
    for case in summary["cases"]:
        if case["issues"]:
            rows += ["", f"## {case['id']} / 회차 {case['repeat']}", ""] + ["- " + s for s in case["issues"]]
    (run / "RESULT.md").write_text("\n".join(rows) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--send", action="store_true", help="Transmit all 8 IDs; receivers may output them on UART")
    parser.add_argument("--source", choices=("D6", "76", "B6"), default="D6")
    parser.add_argument("--peers", nargs="+", choices=("D6", "76", "B6"), default=["76", "B6"])
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--window", type=float, default=2.0)
    parser.add_argument("--out", type=Path, help="New directory, never overwrites existing evidence")
    args = parser.parse_args()
    if args.source in args.peers or len(args.peers) != len(set(args.peers)):
        parser.error("source and peers must be distinct")
    if not 1 <= args.repeat <= 20 or not math.isfinite(args.window) or not 1 <= args.window <= 10:
        parser.error("repeat must be 1..20; window must be finite and 1..10 seconds")
    if args.out:
        run = args.out.resolve()
        run.mkdir(parents=True, exist_ok=False)
    else:
        results = ROOT / "testing/results"
        results.mkdir(exist_ok=True)
        run = Path(tempfile.mkdtemp(prefix="messages-", dir=results))
    print(f"EVIDENCE {run}", flush=True)
    summary = {"source": args.source, "peers": args.peers, "send": args.send,
               "planned_cases": 8 * args.repeat if args.send else 0, "cases": [],
               "verdict": "INCOMPLETE", "relay_test": "NOT_PERFORMED"}
    boards = (args.source, *args.peers)
    try:
        with ExitStack() as stack:
            cap = Capture(stack, run, boards)
            cap.pump(.4)  # Drain already-buffered USB logs before fresh status requests.
            initial = cap.snapshot(boards)
            summary["initial"] = initial
            problems = readiness(initial)
            if problems:
                raise RuntimeError("; ".join(problems))
            start = len(cap.records)
            cap.send("STM32", b"?")
            hello = cap.wait_line("STM32", "MESSAGE_TEST_READY ", start, 3)
            if (hello.get("protocol"), hello.get("ids"), hello.get("uart")) != (
                    "1", "10,11,12,13,20,21,30,31", "USART1"):
                raise RuntimeError("STM32 test firmware protocol mismatch")
            seq = int(hello["seq"])
            summary["firmware"] = hello
            cap.record("HOST", "READY no relay configuration changes; no buttons during test")
            if args.send:
                for repeat in range(1, args.repeat + 1):
                    for ident in MESSAGES:
                        before = cap.snapshot(boards)
                        problems = readiness(before)
                        if problems:
                            raise RuntimeError("; ".join(problems))
                        start = len(cap.records)
                        cap.record("HOST", f"CASE_START id=0x{ident:02X} repeat={repeat}")
                        cap.send("STM32", b"m")
                        cap.wait_line("STM32", "MESSAGE_TEST_ARMED ", start, .8)
                        # This is a binary ID to STM32's one-shot test command, not ASCII hex.
                        cap.send("STM32", bytes([ident]))
                        seq += 1
                        cap.pump(args.window)
                        after = cap.snapshot(boards)
                        case = evaluate(ident, cap.records[start:], before, after, args.source, args.peers, seq)
                        case.update({"repeat": repeat, "before": before, "after": after})
                        summary["cases"].append(case)
                        cap.record("HOST", f"CASE_RESULT id=0x{ident:02X} verdict={case['verdict']}")
                        write_report(run, summary)  # Preserve completed cases even if later interrupted.
                        if case["counts"]["STM32.TX"] != 1:
                            raise RuntimeError("STM32 send not confirmed; stopped without retry")
            summary["final"] = cap.snapshot(boards)
            if not args.send:
                summary["verdict"] = "READY_ONLY_NOT_DELIVERY"
            elif all(c["verdict"] == "PASS_OBSERVED" for c in summary["cases"]):
                summary["verdict"] = "PASS_OBSERVED"
            else:
                summary["verdict"] = "FAIL"
    except (Exception, KeyboardInterrupt) as exc:
        summary["error"] = str(exc) or "Interrupted"
        print("STOP " + summary["error"], file=sys.stderr, flush=True)
    finally:
        write_report(run, summary)
    print(f"{summary['verdict']} — {run / 'RESULT.md'}", flush=True)
    return 0 if summary["verdict"] in ("PASS_OBSERVED", "READY_ONLY_NOT_DELIVERY") else 1


if __name__ == "__main__":
    sys.exit(main())
