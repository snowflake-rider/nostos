"""Synthetic fixtures only: never opens USB or contacts the live Console."""
import copy
from datetime import datetime, timezone
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("mesh_repeat", ROOT / "tools/hardware/mesh_repeat.py")
mesh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mesh)


def state():
    return {"type": "state", "mode": "live", "instance": "fixture-instance", "seq": 10,
            "nodes": [{"board": board, "serial": serial, "phase": "connected", "fresh": True,
                       "status_at": 100000, "error": None,
                       "status": {"name": mesh.NAMES[board], "primary": address,
                                  "net": "0x0000", "app": "0x0001", "pub": "0xc001",
                                  "sub_C001": "1", "event_ready": "1", "ttl": "7", "period": "0",
                                  "retransmit": "0", "relay": "0", "onoff_ready": "1", "state": "0"}}
                      for (board, serial), address in zip(mesh.IDENTITIES.items(), ("0x0005", "0x0003", "0x0006"))]}


def row(board, text, ident=11, when=100.1, direction="rx"):
    return {"type": "log", "log": {"id": ident, "time": datetime.fromtimestamp(when, timezone.utc).isoformat(),
                                    "board": board, "text": text, "direction": direction, "level": "info"}}


def trial():
    t = mesh.Trial(1, "D6", 5, ["76", "B6"], 1, 100, 3)
    t.feed(row("D6", "on-unack · USB에 전달됨", direction="tx")["log"])
    return t


class PreflightTests(unittest.TestCase):
    def test_ready_ignores_cached_relay(self):
        data = state()
        issues, _ = mesh.preflight(data, "D6", ["76", "B6"], now=100)
        self.assertEqual(issues, [])
        data["nodes"][0]["status"]["relay"] = "1"
        self.assertEqual(mesh.preflight(data, "D6", ["76", "B6"], now=100)[0], [])

    def test_rejects_source_c000_not_ready(self):
        data = state()
        data["nodes"][0]["status"]["onoff_ready"] = "0"
        self.assertIn("C000_SOURCE_NOT_READY", " ".join(mesh.preflight(data, "D6", ["76"], now=100)[0]))

    def test_receiver_client_readiness_does_not_claim_server_configuration(self):
        data = state()
        data["nodes"][1]["status"]["onoff_ready"] = "0"
        self.assertFalse(mesh.preflight(data, "D6", ["76"], now=100)[0])

    def test_simulation_stale_identity_and_duplicates_block(self):
        for change in (
            lambda d: d.update(mode="simulation"),
            lambda d: d["nodes"][0].update(status_at=0),
            lambda d: d["nodes"][0].update(status_at=float("nan")),
            lambda d: d["nodes"][0].update(serial="WRONG"),
            lambda d: d["nodes"].append(copy.deepcopy(d["nodes"][0])),
            lambda d: d["nodes"][1]["status"].update(primary="0x0005"),
            lambda d: d["nodes"][0]["status"].update(event_ready="0"),
        ):
            data = state()
            change(data)
            self.assertTrue(mesh.preflight(data, "D6", ["76", "B6"], now=100)[0])

    def test_roles(self):
        for peers, relay in ((["D6"], None), (["76", "76"], None), (["76"], "76"), ([], None)):
            with self.assertRaises(ValueError):
                mesh.roles("D6", peers, relay)


class TrialTests(unittest.TestCase):
    def test_match_and_miss(self):
        t = trial()
        t.feed(row("76", "I (10) LAYER_8_MESH: ONOFF_RX src=0x0005 value=1")["log"])
        self.assertEqual(t.result()["verdict"], "MISSING_RECEIVE")
        t.feed(row("B6", "ONOFF_RX src=0x0005 value=1")["log"])
        self.assertEqual(t.result()["verdict"], "RECEIVE_MATCH_OBSERVED")

    def test_duplicate_wrong_source_wrong_value_and_extra_command(self):
        for text, direction in (("ONOFF_RX src=0x0003 value=1", "rx"),
                                ("ONOFF_RX src=0x0005 value=0", "rx"),
                                ("off-unack · USB에 전달됨", "tx")):
            t = trial()
            t.feed(row("76", text, direction=direction)["log"])
            self.assertEqual(t.result()["verdict"], "AMBIGUOUS")
        t = trial()
        for _ in range(2):
            t.feed(row("76", "ONOFF_RX src=0x0005 value=1")["log"])
        self.assertIn("DUPLICATE_OR_EXTRA_RECEIVE", t.result()["issues"])

    def test_old_log_never_counts_and_late_log_is_ambiguous(self):
        t = trial()
        t.feed(row("76", "ONOFF_RX src=0x0005 value=1", when=99)["log"])
        self.assertEqual(t.result()["counts"]["76"], 0)
        t.feed(row("76", "ONOFF_RX src=0x0005 value=1", when=104)["log"])
        self.assertIn("LATE_RECEIVE", t.result()["issues"])

    def test_api_write_is_not_receive_and_partial_interrupt_is_not_pass(self):
        t = trial()
        self.assertEqual(t.result()["verdict"], "MISSING_RECEIVE")
        self.assertEqual(t.result(interrupted=True)["verdict"], "AMBIGUOUS")
        t.writes = 0
        self.assertEqual(t.result()["verdict"], "AMBIGUOUS")

    def test_millisecond_timestamp_rounding_keeps_own_command(self):
        t = mesh.Trial(1, "D6", 5, ["76"], 1, 100.1239, 3)
        t.feed(row("D6", "on-unack · USB에 전달됨", when=100.123, direction="tx")["log"])
        self.assertEqual(t.writes, 1)


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.clock = patch.object(mesh.time, "time", return_value=100)
        self.clock.start()
        self.addCleanup(self.clock.stop)
        self.guard = mesh.StreamGuard(state(), "D6", ["76", "B6"], None)

    def test_relay_cache_and_onoff_state_changes_are_allowed(self):
        data = state()
        data["nodes"][0]["status"].update(relay="1", state="1")
        self.guard.feed(data, None)

    def test_config_server_restart_disconnect_stale_reject(self):
        for update in (lambda d: d.update(instance="other"),
                       lambda d: d["nodes"][0]["status"].update(app="0x0002"),
                       lambda d: d["nodes"][0].update(phase="disconnected")):
            data = state()
            update(data)
            with self.assertRaises(RuntimeError):
                self.guard.feed(data, None)

    def test_log_gap_old_snapshot_panic_overflow_and_unmatched_traffic(self):
        for event in (row("76", "status", ident=12), {"type": "snapshot"}, {"type": "overflow"},
                      row("76", "BOOT_START"), row("76", "API failed: timeout"),
                      row("76", "ONOFF_RX src=0x0005 value=1")):
            g = mesh.StreamGuard(state(), "D6", ["76", "B6"], None)
            with self.assertRaises(RuntimeError):
                g.feed(event, None)


def phase_report(phase, index):
    on = phase == "relay-on"
    return {"schema": 1, "mode": "live", "test": "C000_ONOFF_LOG_CORRELATION", "phase": phase,
            "run_id": str(index), "started": f"2026-08-28T{index:02}:00:00+00:00",
            "ended": f"2026-08-28T{index:02}:01:00+00:00", "completed": 5,
            "matched": 5 if on else 0, "missing": 0 if on else 5, "ambiguous": 0,
            "peer_received": {"76": 5 if on else 0}, "stop": "COMPLETED", "error": None,
            "source": "D6", "peers": ["76"], "relay": "B6", "conditions": "synthetic fixed layout",
            "isolated_topology_attested": True, "config": {}, "interval": 5, "window": 3}


class CompareTests(unittest.TestCase):
    def compare(self, reports):
        with tempfile.TemporaryDirectory() as d:
            paths = []
            for i, report in enumerate(reports):
                path = Path(d) / f"{i}.json"
                path.write_text(json.dumps(report))
                paths.append(path)
            return mesh.compare(paths)

    def test_effect_is_not_route_proof(self):
        result = self.compare([phase_report(p, i) for i, p in enumerate(("relay-off", "relay-on", "relay-off"))])
        self.assertEqual(result["verdict"], "RELAY_EFFECT_OBSERVED")
        self.assertEqual(result["route_proof"], "NOT_PROVEN")

    def test_bad_baselines_missing_conditions_simulation_and_short_runs(self):
        changes = [lambda r: r[0]["peer_received"].update({"76": 1}),
                   lambda r: r[1].update(conditions="moved"),
                   lambda r: r[2].update(isolated_topology_attested=False),
                   lambda r: r[1].update(mode="simulation"),
                   lambda r: r[2].update(completed=0),
                   lambda r: r[2].update(error="disconnect"),
                   lambda r: r[2].update(run_id="0"),
                   lambda r: r[2].update(started=r[0]["started"]),
                   lambda r: r[1].update(matched=-1),
                   lambda r: r[2].update(stop="INTERRUPTED")]
        for change in changes:
            reports = [phase_report(p, i) for i, p in enumerate(("relay-off", "relay-on", "relay-off"))]
            change(reports)
            self.assertEqual(self.compare(reports)["verdict"], "INCONCLUSIVE")


class ExecutionTests(unittest.TestCase):
    def test_default_no_send_and_bad_arguments_never_contact_hardware(self):
        with patch.object(mesh.Console, "state") as state_call, patch("sys.stderr", io.StringIO()):
            self.assertEqual(mesh.main(["run"]), 2)
            for args in (["--send", "--interval", "nan"], ["--send", "--count", "-1"],
                         ["--send", "--interval", "1"], ["--send", "--phase", "relay-on"]):
                self.assertEqual(mesh.main(["run", *args]), 2)
            state_call.assert_not_called()

    def test_blocked_preflight_never_opens_stream_or_sends(self):
        bad = state()
        bad["nodes"][0]["status"]["onoff_ready"] = "0"
        with patch.object(mesh.time, "time", return_value=100), \
             patch.object(mesh.Console, "state", return_value=bad), \
             patch.object(mesh.Console, "send") as send, patch.object(mesh.Console, "stream") as stream, \
             patch("sys.stderr", io.StringIO()):
            self.assertEqual(mesh.main(["run", "--send"]), 2)
            send.assert_not_called()
            stream.assert_not_called()

    def test_report_preserves_trials_and_closes_with_no_false_pass(self):
        args = mesh.parser().parse_args(["run", "--send"])
        with tempfile.TemporaryDirectory() as d:
            report = mesh.Report(Path(d), args, {})
            with patch("sys.stdout", io.StringIO()):
                report.add(trial().result())
            report.close("ERROR", "disconnected")
            saved = json.loads((Path(d) / "summary.json").read_text())
            self.assertEqual(saved["completed"], 1)
            self.assertEqual(saved["matched"], 0)
            self.assertEqual(saved["stop"], "ERROR")
            self.assertIn("MISSING_RECEIVE", (Path(d) / "trials.csv").read_text())

    def test_console_never_retries_uncertain_post(self):
        client = mesh.Console(8787)
        with patch.object(client, "request", side_effect=TimeoutError("uncertain")) as request:
            with self.assertRaises(TimeoutError):
                client.send("D6", 1)
            self.assertEqual(request.call_count, 1)


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def wall(self):
        return 100 + self.now


class FakeStream:
    def __init__(self, client):
        self.client = client
        self.first = True

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.client.closed = True

    def recv(self, timeout):
        c = self.client
        if self.first:
            self.first = False
            return json.dumps({"type": "snapshot", "state": c.state(),
                               "logs": [row("76", "ONOFF_RX src=0x0005 value=1")["log"]]})
        c.clock.now += .1
        if c.break_at and c.clock.now >= c.break_at:
            raise c.failure("synthetic interrupted stream")
        if c.events:
            return json.dumps(c.events.pop(0))
        return json.dumps(c.state())


class FakeConsole:
    """In-memory simulation; no socket or board is touched."""
    def __init__(self, clock, receive=True, break_at=None, failure=ConnectionError):
        self.clock, self.receive, self.break_at, self.failure = clock, receive, break_at, failure
        self.events, self.sent, self.closed = [], [], False
        self.seq = 10

    def state(self):
        data = state()
        data["seq"] = self.seq
        for node in data["nodes"]:
            node["status_at"] = self.clock.wall() * 1000
        return data

    def stream(self):
        return FakeStream(self)

    def send(self, source, value):
        self.sent.append((source, value))
        self.seq += 1
        self.events.append(row(source, ("on-unack" if value else "off-unack") + " · USB에 전달됨",
                               self.seq, when=self.clock.wall(), direction="tx"))
        if self.receive:
            for peer in ("76", "B6"):
                self.seq += 1
                self.events.append(row(peer, f"ONOFF_RX src=0x0005 value={value}", self.seq,
                                       when=self.clock.wall() + .05))


class FullLoopTests(unittest.TestCase):
    def execute(self, extra=(), receive=True, break_at=None, failure=ConnectionError):
        clock = FakeClock()
        client = FakeConsole(clock, receive, break_at, failure)
        with tempfile.TemporaryDirectory() as d:
            args = mesh.parser().parse_args(["run", "--send", "--count", "2", "--out", str(Path(d) / "run"), *extra])
            with patch.object(mesh.time, "time", side_effect=clock.wall), \
                 patch.object(mesh.time, "monotonic", side_effect=lambda: clock.now), \
                 patch.object(mesh, "runner_lock", return_value=__import__("contextlib").nullcontext()), \
                 patch("sys.stdout", io.StringIO()):
                code = mesh.run(args, client)
            report = json.loads((Path(d) / "run/summary.json").read_text())
            lines = (Path(d) / "run/trials.csv").read_text().splitlines()
            self.assertTrue(client.closed)
        return code, report, client.sent, lines

    def test_repeated_success_ignores_old_snapshot(self):
        code, report, sent, lines = self.execute()
        self.assertEqual(code, 0)
        self.assertEqual(sent, [("D6", 1), ("D6", 0)])
        self.assertEqual(report["matched"], 2)
        self.assertEqual(report["ambiguous"], 0)
        self.assertEqual(report["stop"], "COMPLETED")
        self.assertEqual(len(lines), 3)

    def test_missing_receives_keep_repeating(self):
        code, report, sent, _ = self.execute(receive=False)
        self.assertEqual(code, 0)
        self.assertEqual(report["matched"], 0)
        self.assertEqual(report["missing"], 2)
        self.assertEqual(len(sent), 2)

    def test_disconnect_stops_and_saves_partial_trial(self):
        code, report, sent, _ = self.execute(break_at=2)
        self.assertEqual(code, 2)
        self.assertEqual(report["stop"], "ERROR")
        self.assertEqual(report["ambiguous"], 1)
        self.assertEqual(len(sent), 1)

    def test_ctrl_c_closes_stream_without_extra_off_command(self):
        code, report, sent, _ = self.execute(break_at=2, failure=KeyboardInterrupt)
        self.assertEqual(code, 0)
        self.assertEqual(report["stop"], "INTERRUPTED")
        self.assertEqual(sent, [("D6", 1)])

    def test_continuous_mode_stops_at_duration(self):
        code, report, sent, _ = self.execute(extra=("--count", "0", "--duration", "10"))
        self.assertEqual(code, 0)
        self.assertEqual(report["stop"], "DURATION")
        self.assertEqual(len(sent), 2)


if __name__ == "__main__":
    unittest.main()
