"""Scanner regressions with fake ports/Console only; never contact hardware."""
import contextlib
import importlib.util
import io
import itertools
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("esp32_scan", ROOT / "scripts/esp32_scan.py")
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)


def ports():
    return [SimpleNamespace(serial_number=scan.mesh.IDENTITIES[b], vid=0x303A, pid=0x1001,
                            device=f"/dev/cu.usbmodem{n}") for n, b in enumerate(scan.BOARDS, 1)]


def status(board):
    return {"name": scan.mesh.NAMES[board], "primary": "0x0005", "net": "0x0000", "app": "0x0001",
            "pub": "0xc001", "sub_C001": "1", "event_ready": "1", "ttl": "7", "period": "0",
            "retransmit": "0", "onoff_ready": "0", "state": "0", "relay": "0"}


def state(phase="connected", at=101000):
    return {"type": "state", "mode": "live", "instance": "fixture", "scan_error": None,
            "nodes": [{"board": b, "serial": p.serial_number, "path": p.device, "error": None,
                       "phase": phase, "fresh": True, "status_at": at, "status": status(b)}
                      for b, p in zip(scan.BOARDS, ports())]}


class FakeStream:
    def __init__(self, initial, events):
        self.events = iter([{"type": "snapshot", "state": initial, "logs": [{"secret": "DO_NOT_ECHO"}]}, *events])
        self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.closed = True

    def recv(self, timeout):
        try:
            value = next(self.events)
        except StopIteration:
            raise TimeoutError()
        if isinstance(value, BaseException):
            raise value
        return json.dumps(value)


class ScannerTests(unittest.TestCase):
    def run_scan(self, initial=None, events=None, **options):
        stream = FakeStream(initial or state(), [state()] if events is None else events)
        console = Mock()
        console.stream.return_value = stream
        clock = itertools.count(0, .2)
        with patch.object(scan.time, "time", side_effect=itertools.chain([100], itertools.repeat(102))), \
                patch.object(scan.time, "monotonic", side_effect=lambda: next(clock)):
            report = scan.scan(ports(), console, timeout=2, port_guard=options.pop("port_guard", Mock()), **options)
        self.assertTrue(stream.closed)
        return report, console

    def test_inventory_uses_serial_and_vid_pid_and_ignores_other_devices(self):
        fixture = ports()
        fixture.append(SimpleNamespace(serial_number="unrelated", vid=0x303A, pid=0x1001, device="/dev/ignore"))
        self.assertEqual(len(scan.inventory(fixture)), 3)
        fixture[0].pid = 0x9999
        self.assertEqual(scan.inventory(fixture)[0]["status_result"], "USB_NOT_FOUND")

    def test_duplicate_and_unexpected_path_are_not_openable(self):
        fixture = ports()
        self.assertEqual(scan.inventory(fixture + [fixture[0]])[0]["status_result"], "DUPLICATE_USB_IDENTITY")
        fixture[0].device = "/dev/cu.Bluetooth-Incoming-Port"
        self.assertEqual(scan.inventory(fixture)[0]["status_result"], "UNEXPECTED_USB_PATH")

    def test_missing_boards_do_not_contact_console(self):
        console = Mock()
        report = scan.scan([], console)
        self.assertEqual(report["result"], "INCOMPLETE")
        console.stream.assert_not_called()

    def test_auto_prepare_only_for_eligible_status_queries(self):
        prepare = Mock()
        self.run_scan(prepare_console=prepare)
        prepare.assert_called_once_with()
        prepare.reset_mock()
        scan.scan([], Mock(), prepare_console=prepare)
        scan.scan(ports(), Mock(), usb_only=True, prepare_console=prepare)
        self.run_scan(connect_missing=False, prepare_console=prepare)
        prepare.assert_not_called()

    def test_prepare_failure_is_safe_json_without_device_access(self):
        for failure, code in ((RuntimeError("CONSOLE_START_FAILED"), "CONSOLE_START_FAILED"),
                              (OSError("secret detail"), "CONSOLE_UNAVAILABLE_OR_STREAM_ERROR"),
                              (KeyboardInterrupt(), "INTERRUPTED")):
            console = Mock()
            report = scan.scan(ports(), console, prepare_console=Mock(side_effect=failure))
            self.assertEqual(report["error"], code)
            self.assertEqual(report["result"], "INCOMPLETE")
            self.assertNotIn("secret detail", json.dumps(report))
            console.stream.assert_not_called()

    def test_usb_only_never_connects(self):
        console = Mock()
        report = scan.scan(ports(), console, usb_only=True)
        self.assertEqual(report["result"], "USB_ONLY")
        console.stream.assert_not_called()
        console.request.assert_not_called()

    def test_connected_boards_reused_and_new_status_required(self):
        report, console = self.run_scan(initial=state(at=99000), events=[state(at=99000), state()])
        self.assertEqual(report["result"], "STATUS_READ_PARTIAL_CONFIG")
        console.request.assert_not_called()
        self.assertNotIn("DO_NOT_ECHO", json.dumps(report))
        self.assertEqual(report["mesh_test_transmissions_sent"], 0)

    def test_disconnected_boards_only_use_connect_not_command(self):
        guard = Mock()
        report, console = self.run_scan(initial=state("disconnected"), port_guard=guard)
        self.assertEqual(report["result"], "STATUS_READ_PARTIAL_CONFIG")
        self.assertEqual([c.args for c in console.request.call_args_list],
                         [(f"/api/boards/{b}/connect", {}) for b in scan.BOARDS])
        self.assertEqual(guard.call_count, 3)
        console.send.assert_not_called()

    def test_no_connect_and_busy_port_never_post(self):
        report, console = self.run_scan(initial=state("disconnected"), connect_missing=False)
        self.assertTrue(all(d["status_result"] == "CONSOLE_NOT_CONNECTED" for d in report["devices"]))
        console.request.assert_not_called()
        report, console = self.run_scan(initial=state("disconnected"),
                                        port_guard=Mock(side_effect=RuntimeError("USB_IN_USE_BY_ANOTHER_PROCESS")))
        self.assertTrue(all(d["status_result"] == "USB_IN_USE_BY_ANOTHER_PROCESS" for d in report["devices"]))
        self.assertEqual([call.args for call in console.request.call_args_list], [("/api/state",)] * 3)

    def test_verifying_connection_is_shared_without_another_open(self):
        report, console = self.run_scan(initial=state("verifying"))
        self.assertEqual(report["result"], "STATUS_READ_PARTIAL_CONFIG")
        console.request.assert_not_called()

    def test_concurrent_query_reuses_same_server_after_port_guard_race(self):
        for restarted in (False, True):
            stream = FakeStream(state("disconnected"), [state()])
            console = Mock()
            console.stream.return_value = stream
            console.request.return_value = {**state(), "instance": "new" if restarted else "fixture"}
            with patch.object(scan.time, "time", side_effect=itertools.chain([100], itertools.repeat(102))):
                report = scan.scan(ports(), console,
                                   port_guard=Mock(side_effect=RuntimeError("USB_IN_USE_BY_ANOTHER_PROCESS")))
            self.assertEqual(report["result"], "INCOMPLETE" if restarted else "STATUS_READ_PARTIAL_CONFIG")
            self.assertTrue(all(call.args == ("/api/state",) for call in console.request.call_args_list))

    def test_nonlive_and_identity_mismatch_are_blocked(self):
        fixture = state("disconnected")
        fixture["mode"] = "simulation"
        report, console = self.run_scan(initial=fixture)
        self.assertEqual(report["error"], "CONSOLE_NOT_LIVE")
        console.request.assert_not_called()
        fixture = state("disconnected")
        fixture["nodes"][0]["serial"] = "WRONG"
        report, console = self.run_scan(initial=fixture)
        self.assertEqual(report["devices"][0]["status_result"], "CONSOLE_USB_IDENTITY_MISMATCH")
        self.assertNotIn("/api/boards/D6/connect", [c.args[0] for c in console.request.call_args_list])

    def test_stale_null_future_and_bad_timestamp_never_pass(self):
        for at in (None, 99000, float("nan"), 9999999999999):
            fixture = state(at=at)
            with self.subTest(at=at):
                report, _ = self.run_scan(events=[fixture])
                self.assertTrue(all(d["status_result"] == "STATUS_TIMEOUT" for d in report["devices"]))

    def test_disconnect_interrupt_and_server_restart_preserve_uncertainty(self):
        for event in (OSError("private response not for output"), KeyboardInterrupt(),
                      {**state(), "instance": "new-instance"}):
            report, _ = self.run_scan(events=[event])
            self.assertEqual(report["result"], "INCOMPLETE")
            self.assertNotIn("private response", json.dumps(report))

    def test_partial_results_are_not_all_board_success(self):
        fixture = state()
        fixture["nodes"][1]["status_at"] = None
        report, _ = self.run_scan(events=[fixture])
        self.assertEqual(report["result"], "INCOMPLETE")
        self.assertEqual([d["status_result"] for d in report["devices"]], ["READ", "STATUS_TIMEOUT", "READ"])

    def test_unknown_and_cached_fields_are_explicit_and_keys_are_filtered(self):
        fixture = state()
        for node in fixture["nodes"]:
            node["status"]["appkey"] = "private-key-bytes"
        report, _ = self.run_scan(events=[fixture])
        self.assertNotIn("private-key-bytes", json.dumps(report))
        self.assertIn("onoff_server_subscriptions_and_bindings", report["unavailable_fields"])
        self.assertIsNone(report["configuration_snapshot_updated_at"])
        result = report["devices"][0]["firmware_report"]
        self.assertIn("relay_cached", result)
        self.assertNotIn("relay", result)
        self.assertEqual(result["onoff_ready"], "0")  # Reading succeeded despite unready transmitter.

    def test_status_schema_rejects_wrong_firmware_missing_fields_or_secret_like_values(self):
        for key, value in (("name", "other-firmware"), ("net", "ab" * 16), ("event_ready", "2"),
                           ("ttl", "9999"), ("relay", "3"), ("state", None)):
            fixture = status("D6")
            fixture[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                scan.safe_status(fixture, "D6")

    def test_port_ownership_checks_both_aliases_and_fails_closed(self):
        with patch.object(scan.subprocess, "run", return_value=SimpleNamespace(returncode=1, stderr="")) as run:
            scan.require_free_port("/dev/cu.usbmodem123")
            self.assertIn("/dev/tty.usbmodem123", run.call_args.args[0])
        for result in (SimpleNamespace(returncode=0, stderr=""), SimpleNamespace(returncode=2, stderr="error")):
            with patch.object(scan.subprocess, "run", return_value=result), self.assertRaises(RuntimeError):
                scan.require_free_port("/dev/cu.usbmodem123")

    def test_cli_help_and_invalid_options_never_require_hardware(self):
        result = subprocess.run(["bash", str(ROOT / "scripts/esp32-scan"), "--help"],
                                cwd="/tmp", text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        for args in (["--send"], ["--timeout", "nan"], ["--timeout", "0"], ["--port", "0"]):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
                scan.main(args)
            self.assertEqual(caught.exception.code, 2)

    def test_existing_output_refused_before_usb_enumeration(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "scan.json"
            output.write_text("preserve me")
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                scan.main(["--out", str(output)])
            self.assertEqual(output.read_text(), "preserve me")


if __name__ == "__main__":
    unittest.main()
