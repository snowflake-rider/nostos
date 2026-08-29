#!/usr/bin/env python3
"""C000 repeat/soak tests through an already connected local Mesh Console.

No serial opens, flash, reset, keys, Relay writes, C001 injection or auto retries.
Matching logs are observations, NOT packet identities, ACKs or route proof.
"""
import argparse
from contextlib import contextmanager
import csv
from datetime import datetime, timezone
import fcntl
import json
import math
import os
from pathlib import Path
import re
import sys
import tempfile
import time
from urllib.error import HTTPError
from urllib.request import Request, ProxyHandler, build_opener
import uuid

ROOT = Path(__file__).resolve().parents[2]
IDENTITIES = {"D6": "14:C1:9F:CE:F0:D4", "76": "14:C1:9F:CE:EC:74", "B6": "44:1B:F6:FF:BA:B4"}
NAMES = {"D6": "ESP32-L8-F0D6", "76": "ESP32-L8-EC76", "B6": "ESP32-L8-BAB6"}
CONFIG = ("name", "primary", "net", "app", "pub", "sub_C001", "event_ready",
          "ttl", "period", "retransmit", "onoff_ready")
RX = re.compile(r"\bONOFF_RX src=0x([0-9a-fA-F]+) value=([01])\b")
LIMITS = ["C000 only; not C001/UART/STM32 validation",
          "No wire sequence in ONOFF_RX logs; correlation is not packet identity or ACK",
          "Cached relay status is not a current Relay readback",
          "No measured RF latency, exact packet loss rate or multi-hop route proof"]


def human_output():
    return os.environ.get("MESH_REPEAT_HUMAN") == "1"


def display_check(issues, config):
    print("BLOCKED — 송신 전 준비 필요" if issues else "READY — 송신 준비 보고 확인 (수신 성공 아님)")
    for board, values in config.items():
        print(f"  {board}: 주소={values.get('primary', '?')} C001={values.get('event_ready', '?')} "
              f"OnOff송신준비={values.get('onoff_ready', '?')}")
    for issue in issues:
        print("  확인: " + issue)
    if any("C000_SOURCE_NOT_READY" in issue for issue in issues):
        print("  다음: 송신 노드 Client의 AppKey Bind + Publication C000 확인.")
        print("  앱에서 이미 확인했다면 재설정하지 말고 펌웨어 준비 판정/캐시 진단.")
    print("  수신 노드 Server의 AppKey Bind + C000 구독은 앱에서 별도 확인.")
    print("  UART·STM32 출력·현재 Relay·다중 홉: 미검증")


def run_result(summary, phase):
    """Human stage exit codes. Relay-OFF missing reception is expected evidence."""
    if summary["stop"] == "INTERRUPTED":
        return "CANCELLED", 130
    if summary.get("error") or summary["ambiguous"] or summary["stop"] != "COMPLETED" or not summary["completed"]:
        return "INCONCLUSIVE", 2
    if summary["missing"] and phase == "delivery":
        return "FAIL", 1
    return "OBSERVED", 0


def utc():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def stamp(text):
    return datetime.fromisoformat(text.replace("Z", "+00:00")).timestamp()


def roles(source, peers, relay):
    boards = [source, *peers] + ([relay] if relay else [])
    if not peers or len(boards) != len(set(boards)) or any(b not in IDENTITIES for b in boards):
        raise ValueError("source/peers/relay must be distinct known boards; at least one peer required")
    return boards


def preflight(state, source, peers, relay=None, now=None):
    """C000 receiver binding/subscription isn't exposed by existing status."""
    now = time.time() if now is None else now
    issues, selected, addresses = [], {}, []
    if state.get("mode") != "live":
        issues.append("NOT_LIVE: simulation cannot be used for a hardware run")
    if state.get("scan_error"):
        issues.append("USB_SCAN_ERROR")
    for board in roles(source, peers, relay):
        matches = [n for n in state.get("nodes", []) if n.get("board") == board]
        if len(matches) != 1:
            issues.append(f"{board}: missing/duplicate node")
            continue
        n = matches[0]
        s = n.get("status") or {}
        selected[board] = {k: s.get(k) for k in CONFIG}
        try:
            age = now - float(n["status_at"]) / 1000
            if not math.isfinite(age) or not -1 <= age < 15:
                raise ValueError("stale timestamp")
            if n.get("phase") != "connected" or n.get("fresh") is not True or n.get("error"):
                raise ValueError("disconnected/stale/error")
            if n.get("serial") != IDENTITIES[board] or s.get("name") != NAMES[board]:
                raise ValueError("USB/firmware identity mismatch")
            address = int(s["primary"], 16)
            if not 0 < address < 0x8000:
                raise ValueError("invalid unicast")
            addresses.append(address)
            if any(not 0 <= int(s[k], 16) <= 0xFFF for k in ("app", "net")):
                raise ValueError("invalid key index")
            expected = {"event_ready": "1", "sub_C001": "1", "ttl": "7", "period": "0", "retransmit": "0"}
            if any(s.get(k) != v for k, v in expected.items()) or int(s["pub"], 16) != 0xC001:
                raise ValueError("existing C001 configuration not ready")
            if board == source and s.get("onoff_ready") != "1":
                issues.append(f"{board}: C000_SOURCE_NOT_READY (Generic OnOff Client bind/publication needed)")
        except (ValueError, TypeError, KeyError) as exc:
            issues.append(f"{board}: {exc}")
    if len(addresses) != len(set(addresses)):
        issues.append("duplicate unicast addresses")
    return issues, selected


class Console:
    def __init__(self, port):
        self.base = f"http://127.0.0.1:{port}"
        self.opener = build_opener(ProxyHandler({}))  # Never route local device data via a proxy.

    def request(self, path, payload=None):
        data = None if payload is None else json.dumps(payload).encode()
        req = Request(self.base + path, data=data, headers={"Origin": self.base, "Content-Type": "application/json"})
        try:
            with self.opener.open(req, timeout=3) as response:
                return json.load(response)
        except HTTPError as exc:
            detail = exc.read(4096).decode(errors="replace")
            raise RuntimeError(f"HTTP {exc.code}: {detail}; no retry") from exc

    def state(self):
        return self.request("/api/state")

    def send(self, source, value):
        command = "on-unack" if value else "off-unack"
        result = self.request(f"/api/boards/{source}/command", {"command": command})
        if result.get("result") != "written" or result.get("board") != source or result.get("command") != command:
            raise RuntimeError("Unconfirmed USB write; stopped without retry")

    def stream(self):
        from websockets.sync.client import connect
        return connect(self.base.replace("http:", "ws:") + "/api/stream", origin=self.base,
                       proxy=None, open_timeout=5, close_timeout=2, max_size=8 * 1024 * 1024, max_queue=64)


class Trial:
    def __init__(self, number, source, address, peers, value, started, window):
        self.number, self.source, self.address, self.value = number, source, address, value
        # Console timestamps have millisecond precision. Don't discard our own
        # command just because its timestamp was rounded down within this ms.
        self.started = math.floor(started * 1000) / 1000
        self.ends = self.started + window
        self.counts = dict.fromkeys(peers, 0)
        self.issues = set()
        self.writes = 0

    def feed(self, row):
        board, text = row.get("board"), row.get("text", "")
        when = stamp(row["time"])
        if when < self.started:
            return
        if row.get("direction") == "tx":
            wanted = "on-unack" if self.value else "off-unack"
            command = text.split(" ", 1)[0]
            if command == "status":
                return
            if board == self.source and command == wanted and when <= self.ends:
                self.writes += 1
            else:
                self.issues.add("OTHER_COMMAND_DURING_TRIAL")
        match = RX.search(text)
        if not match:
            return
        if when > self.ends:
            self.issues.add("LATE_RECEIVE")
        elif int(match[1], 16) != self.address or int(match[2]) != self.value:
            self.issues.add("UNEXPECTED_SOURCE_OR_VALUE")
        elif board in self.counts:
            self.counts[board] += 1

    def result(self, interrupted=False):
        issues = set(self.issues)
        if self.writes != 1:
            issues.add("EXPECTED_ONE_USB_COMMAND_LOG")
        if any(n > 1 for n in self.counts.values()):
            issues.add("DUPLICATE_OR_EXTRA_RECEIVE")
        if interrupted:
            issues.add("INTERRUPTED_TRIAL")
        verdict = "AMBIGUOUS" if issues else (
            "RECEIVE_MATCH_OBSERVED" if all(n == 1 for n in self.counts.values()) else "MISSING_RECEIVE")
        return {"number": self.number, "value": self.value, "started": self.started,
                "counts": self.counts, "verdict": verdict, "issues": sorted(issues)}


class StreamGuard:
    """Reject old snapshots, log gaps, config changes and any broken observation window."""
    def __init__(self, state, source, peers, relay):
        issues, self.config = preflight(state, source, peers, relay)
        if issues:
            raise RuntimeError("; ".join(issues))
        self.source, self.peers, self.relay = source, peers, relay
        self.instance, self.last_id = state["instance"], state["seq"]

    def feed(self, event, trial):
        kind = event.get("type")
        if kind == "state":
            issues, config = preflight(event, self.source, self.peers, self.relay)
            if issues or event.get("instance") != self.instance or config != self.config:
                raise RuntimeError("STATE_CHANGED_OR_NOT_READY: " + "; ".join(issues))
        elif kind == "log":
            row = event["log"]
            if row["id"] != self.last_id + 1:
                raise RuntimeError("LOG_GAP_OR_REORDER; stop instead of silently reconnecting")
            self.last_id = row["id"]
            text = row.get("text", "")
            if any(m in text for m in ("BOOT_START", "APP_STARTED", "UART1_READY", "panic", "Guru Meditation")):
                raise RuntimeError("DEVICE_RESTART_OR_PANIC")
            if row.get("level") == "error" or "API failed" in text or "Command rejected" in text:
                raise RuntimeError("DEVICE_OR_CONSOLE_ERROR")
            if trial is not None:
                trial.feed(row)
            elif RX.search(text) or (row.get("direction") == "tx" and not text.startswith("status")):
                raise RuntimeError("UNMATCHED_TRAFFIC_OR_OTHER_OPERATOR; wait for a quiet network")
        else:
            raise RuntimeError("UNEXPECTED_STREAM_EVENT: " + str(kind))


class Report:
    def __init__(self, path, args, initial):
        self.path = path
        self.events = (path / "events.jsonl").open("x", encoding="utf-8")
        self.trials = (path / "trials.csv").open("x", encoding="utf-8", newline="")
        self.csv = csv.writer(self.trials)
        self.csv.writerow(["trial", "value", "verdict", *args.peers, "issues"])
        self.summary = {"schema": 1, "mode": "live", "test": "C000_ONOFF_LOG_CORRELATION",
                        "run_id": str(uuid.uuid4()), "started": utc(), "ended": None,
                        "source": args.source, "peers": args.peers, "relay": args.relay,
                        "phase": args.phase, "conditions": args.conditions,
                        "isolated_topology_attested": args.confirm_isolated_topology,
                        "interval": args.interval, "window": args.window, "config": initial,
                        "completed": 0, "matched": 0, "missing": 0, "ambiguous": 0,
                        "peer_received": dict.fromkeys(args.peers, 0),
                        "stop": "RUNNING", "error": None, "limitations": LIMITS}
        self.event("start", {"config": initial})
        self.save()

    def event(self, kind, value):
        self.events.write(json.dumps({"time": utc(), "kind": kind, "data": value}, ensure_ascii=False) + "\n")
        self.events.flush()

    def add(self, case):
        s = self.summary
        s["completed"] += 1
        key = {"RECEIVE_MATCH_OBSERVED": "matched", "MISSING_RECEIVE": "missing", "AMBIGUOUS": "ambiguous"}[case["verdict"]]
        s[key] += 1
        for peer, count in case["counts"].items():
            s["peer_received"][peer] += int(count > 0)
        self.csv.writerow([case["number"], case["value"], case["verdict"],
                           *case["counts"].values(), ";".join(case["issues"])])
        self.trials.flush()
        self.event("trial", case)
        self.save()
        if human_output():
            print(f"#{case['number']} 수신일치={s['matched']}/{s['completed']} "
                  f"누락={s['missing']} 모호={s['ambiguous']}", flush=True)
        else:
            print(f"#{case['number']} value={case['value']} {case['verdict']} {case['counts']} "
                  f"matched={s['matched']}/{s['completed']}", flush=True)

    def save(self):
        # One small current summary; trials/events are flushed append-only, not kept in RAM.
        temp = self.path / "summary.tmp"
        temp.write_text(json.dumps(self.summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        temp.replace(self.path / "summary.json")

    def size(self):
        return self.events.tell() + self.trials.tell()

    def close(self, reason, error=None):
        self.summary.update(stop=reason, error=error, ended=utc())
        self.save()
        self.events.close()
        self.trials.close()


@contextmanager
def runner_lock(port):
    directory = ROOT / "build/hardware-results"
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / f".mesh-repeat-{port}.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise RuntimeError("Another repeat test owns this checkout/server; not starting") from exc
        yield


def run(args, console):
    if not args.send:
        raise ValueError("run requires --send; it changes C000 OnOff state. Use check for read-only preflight")
    # No evidence directory or active session until dependencies and readiness pass.
    issues, initial = preflight(console.state(), args.source, args.peers, args.relay)
    if issues:
        raise RuntimeError("; ".join(issues))
    with runner_lock(args.port), console.stream() as stream:
        first = json.loads(stream.recv(timeout=5))
        if first.get("type") != "snapshot":
            raise RuntimeError("Missing initial snapshot")
        guard = StreamGuard(first["state"], args.source, args.peers, args.relay)
        if guard.config != initial:
            raise RuntimeError("Configuration changed during preflight")
        # Deliberately never replay first['logs']; they predate this test.
        if args.out:
            path = args.out.resolve()
            path.mkdir(parents=True, exist_ok=False)
        else:
            path = Path(tempfile.mkdtemp(prefix="mesh-repeat-", dir=ROOT / "build/hardware-results"))
        report = Report(path, args, initial)
        print(f"EVIDENCE {path}\nC000 only. Ctrl-C stops; no state restoration command is sent.", flush=True)
        started = time.monotonic()
        next_send = started + 1  # Quiet observation before the first command.
        last_state = started
        active, settle_end, reason, error = None, 0.0, "COMPLETED", None
        try:
            while True:
                now = time.monotonic()
                if now - last_state > 4:
                    raise RuntimeError("STATE_STREAM_STALLED")
                if report.size() >= args.max_log_mb * 1024 * 1024:
                    reason = "LOG_LIMIT"
                    break
                if args.duration and now - started >= args.duration:
                    reason = "DURATION"
                    break
                if active is not None and now >= settle_end:
                    case = active.result()
                    report.add(case)
                    active = None
                    if case["verdict"] == "AMBIGUOUS":
                        raise RuntimeError("AMBIGUOUS_TRIAL; cannot attribute traffic safely")
                if active is None and args.count and report.summary["completed"] >= args.count:
                    break
                if active is None and now >= next_send:
                    # A current HTTP read protects the next write, even if old state events are queued.
                    current = console.state()
                    guard.feed(current, None)
                    now = last_state = time.monotonic()
                    number = report.summary["completed"] + 1
                    active = Trial(number, args.source, int(initial[args.source]["primary"], 16),
                                   args.peers, number % 2, time.time(), args.window)
                    report.event("command_intent", {"number": number, "source": args.source, "value": active.value})
                    console.send(args.source, active.value)  # Never retry uncertain writes.
                    next_send = now + args.interval
                    settle_end = now + args.window + .5  # Drain USB/stream tail before finalizing.
                try:
                    event = json.loads(stream.recv(timeout=.1))
                except TimeoutError:
                    continue
                guard.feed(event, active)
                if event.get("type") == "state":
                    last_state = time.monotonic()
                elif event.get("type") == "log":
                    row = event["log"]
                    # Persist only task-relevant log lines; never dump key/unknown firmware output.
                    if RX.search(row.get("text", "")) or row.get("direction") == "tx":
                        report.event("log", {k: row[k] for k in ("id", "time", "board", "direction", "text")})
        except KeyboardInterrupt:
            reason = "INTERRUPTED"
        except Exception as exc:
            reason, error = "ERROR", str(exc)
        finally:
            if active is not None:
                report.add(active.result(interrupted=True))
            report.close(reason, error)
        if human_output():
            verdict, code = run_result(report.summary, args.phase)
            print(f"{verdict} — 수신일치 {report.summary['matched']}/{report.summary['completed']}, "
                  f"누락 {report.summary['missing']}, 모호 {report.summary['ambiguous']}; 종료={reason}")
            if error:
                print("원인: " + error)
            print(f"상세: {path / 'summary.json'}\nC000 로그 관찰만; ACK·정확한 경로·STM32 동작은 미검증.", flush=True)
            return code
        print(f"STOP={reason} {error or ''}\nSUMMARY {path / 'summary.json'}", flush=True)
        return 2 if error or reason == "LOG_LIMIT" else 0


def compare(paths):
    reports = [json.loads(p.read_text(encoding="utf-8")) for p in paths]
    issues = []
    for r, phase in zip(reports, ("relay-off", "relay-on", "relay-off")):
        if r.get("schema") != 1 or r.get("mode") != "live" or r.get("test") != "C000_ONOFF_LOG_CORRELATION":
            issues.append("incompatible or simulated report")
        if r.get("phase") != phase or r.get("isolated_topology_attested") is not True:
            issues.append("OFF/ON/OFF order and operator topology attestation required")
        if not r.get("relay") or not r.get("conditions") or len(r.get("peers", [])) != 1:
            issues.append("one receiver, distinct relay and unchanged layout note required")
        n = r.get("completed", 0)
        if not isinstance(n, int) or n < 5 or r.get("ambiguous") != 0 or r.get("error") or r.get("stop") != "COMPLETED":
            issues.append("need >=5 complete unambiguous trials per finite run")
        counts = [r.get(k) for k in ("matched", "missing", "ambiguous")]
        if any(type(x) is not int or x < 0 for x in counts) or sum(x for x in counts if type(x) is int) != n:
            issues.append("invalid aggregate counts")
        for peer in r.get("peers", []):
            hits = r.get("peer_received", {}).get(peer)
            if type(hits) is not int or hits != (n if phase == "relay-on" else 0):
                issues.append("expected zero target receptions OFF and all receptions ON")
            if hits != r.get("matched"):
                issues.append("inconsistent single-peer counts")
    fixed = ("source", "peers", "relay", "conditions", "interval", "window", "config", "completed")
    if any(any(r.get(k) != reports[0].get(k) for k in fixed) for r in reports[1:]):
        issues.append("roles/config/layout/timing/trial counts differ across runs")
    try:
        for before, after in zip(reports, reports[1:]):
            if stamp(before["ended"]) > stamp(after["started"]):
                issues.append("runs overlap or are out of time order")
        if len({r["run_id"] for r in reports}) != 3:
            issues.append("three independent runs required")
    except (KeyError, ValueError, TypeError):
        issues.append("missing/invalid run timestamps or identities")
    return {"verdict": "INCONCLUSIVE" if issues else "RELAY_EFFECT_OBSERVED",
            "issues": sorted(set(issues)), "route_proof": "NOT_PROVEN",
            "explanation": "OFF/ON/OFF correlation under operator-declared isolation; not an exact packet route proof"}


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="action", required=True)
    check = sub.add_parser("check", help="read-only live preflight; no serial or Mesh writes")
    run_p = sub.add_parser("run", help="explicit C000 ON/OFF repeat; requires --send")
    for child in (check, run_p):
        child.add_argument("--port", type=int, default=8787)
        child.add_argument("--source", choices=IDENTITIES, default="D6")
        child.add_argument("--peers", nargs="+", choices=IDENTITIES, default=["76", "B6"])
        child.add_argument("--relay", choices=IDENTITIES)
    run_p.add_argument("--send", action="store_true")
    run_p.add_argument("--count", type=int, default=20, help="0 = until Ctrl-C/duration/log cap")
    run_p.add_argument("--duration", type=float, default=0, help="seconds; 0 = no time limit")
    run_p.add_argument("--interval", type=float, default=5)
    run_p.add_argument("--window", type=float, default=3)
    run_p.add_argument("--max-log-mb", type=float, default=50)
    run_p.add_argument("--out", type=Path)
    run_p.add_argument("--phase", choices=("delivery", "relay-off", "relay-on"), default="delivery")
    run_p.add_argument("--conditions", default="", help="fixed topology/distances/power/TTL notes, not keys")
    run_p.add_argument("--confirm-isolated-topology", action="store_true",
                       help="operator has checked selected Relay state, other relays OFF, fixed layout, no other senders")
    comp = sub.add_parser("compare", help="offline comparison; OFF ON OFF summary.json paths")
    comp.add_argument("reports", type=Path, nargs=3)
    return p


def main(argv=None):
    p = parser()
    args = p.parse_args(argv)
    try:
        if args.action == "compare":
            result = compare(args.reports)
            print(json.dumps(result, ensure_ascii=False, indent=2))
            return 0 if result["verdict"] == "RELAY_EFFECT_OBSERVED" else 2
        roles(args.source, args.peers, args.relay)
        if not 1 <= args.port <= 65535:
            raise ValueError("port must be 1..65535")
        console = Console(args.port)
        if args.action == "check":
            state = console.state()
            issues, config = preflight(state, args.source, args.peers, args.relay)
            result = {"verdict": "BLOCKED" if issues else "READY_ONLY_NOT_DELIVERY", "issues": issues,
                              "config": config, "limitations": LIMITS,
                              "receiver_requirement": "Generic OnOff Server AppKey bind + C000 subscription; not visible in current USB status"}
            if human_output():
                display_check(issues, config)
            else:
                print(json.dumps(result, ensure_ascii=False, indent=2))
            return 2 if issues else 0
        if args.count < 0 or any(not math.isfinite(v) for v in (args.interval, args.window, args.duration, args.max_log_mb)):
            raise ValueError("count must be nonnegative; times and log cap must be finite")
        if not 1 <= args.window <= 30 or not max(5, args.window + 1) <= args.interval <= 300 or args.duration < 0 or not 1 <= args.max_log_mb <= 1024:
            raise ValueError("window 1..30; interval >=max(5,window+1) and <=300; duration >=0; log cap 1..1024 MiB")
        if args.phase != "delivery" and (len(args.peers) != 1 or not args.relay or not args.conditions.strip() or not args.confirm_isolated_topology):
            raise ValueError("relay phase needs one peer, --relay, --conditions and --confirm-isolated-topology")
        return run(args, console)
    except (Exception, KeyboardInterrupt) as exc:
        print("STOP: " + (str(exc) or "Interrupted"), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
