"""Auto-start policy tests; fake HTTP/processes, no hardware access."""
import http.client
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("esp32_console", ROOT / "scripts/esp32_console.py")
console = importlib.util.module_from_spec(spec)
spec.loader.exec_module(console)


def state():
    return {"type": "state", "mode": "live", "instance": "a" * 32,
            "nodes": [{"board": b, "serial": s} for b, s in console.IDENTITIES.items()]}


class ConsoleTests(unittest.TestCase):
    def probe(self, data=None, status=200, failure=None, body=None):
        connection = Mock()
        connection.request.side_effect = failure
        response = connection.getresponse.return_value
        response.status = status
        response.read.return_value = body if body is not None else json.dumps(data or state()).encode()
        with patch.object(console.http.client, "HTTPConnection", return_value=connection) as connect:
            try:
                return console.probe_console(12345)
            finally:
                connect.assert_called_once_with("127.0.0.1", 12345, timeout=.75)
                connection.close.assert_called_once()

    def test_probe_validates_live_identity(self):
        self.assertEqual(self.probe(), "a" * 32)
        for data in ({**state(), "mode": "test"}, {**state(), "instance": "wrong"},
                     {**state(), "nodes": []}, {**state(), "nodes": [None] * 3},
                     {**state(), "nodes": [state()["nodes"][0]] * 3}):
            with self.subTest(data=data), self.assertRaises(RuntimeError):
                self.probe(data)

    def test_only_connection_refused_allows_start(self):
        self.assertIsNone(self.probe(failure=ConnectionRefusedError()))
        for failure in (TimeoutError(), ConnectionResetError(), http.client.BadStatusLine("private")):
            with self.subTest(failure=failure), self.assertRaises(RuntimeError):
                self.probe(failure=failure)

    def test_foreign_redirect_malformed_and_oversized_responses_fail_closed(self):
        for options in ({"status": 302}, {"status": 503}, {"body": b"private"},
                        {"body": b"x" * (128 * 1024 + 1)}, {"body": b"[]"}):
            with self.subTest(options=list(options)), self.assertRaisesRegex(RuntimeError, "CONSOLE_PORT_NOT_COMPATIBLE"):
                self.probe(**options)

    def test_existing_server_is_reused_without_start_or_stop(self):
        launch = Mock()
        self.assertEqual(console.ensure_console(12345, probe=Mock(return_value="existing"), launch=launch), "existing")
        launch.assert_not_called()

    def test_foreign_server_never_launches(self):
        launch = Mock()
        with self.assertRaisesRegex(RuntimeError, "CONSOLE_NOT_LIVE"):
            console.ensure_console(12345, probe=Mock(side_effect=RuntimeError("CONSOLE_NOT_LIVE")), launch=launch)
        launch.assert_not_called()

    def test_refused_connection_starts_once(self):
        launch = Mock()
        self.assertEqual(console.ensure_console(12345, probe=Mock(side_effect=[None, "started"]), launch=launch), "started")
        launch.assert_called_once_with(12345)
        launch.return_value.terminate.assert_not_called()

    def test_concurrent_loser_waits_for_winner(self):
        child = Mock()
        child.poll.return_value = 75
        with patch.object(console.time, "sleep"):
            self.assertEqual(console.ensure_console(12345, probe=Mock(side_effect=[None, None, "winner"]),
                                                   launch=Mock(return_value=child)), "winner")
        child.kill.assert_not_called()

    def test_failed_start_and_timeout_are_bounded(self):
        child = Mock()
        child.poll.return_value = 1
        with self.assertRaisesRegex(RuntimeError, "CONSOLE_START_FAILED"):
            console.ensure_console(12345, probe=Mock(return_value=None), launch=Mock(return_value=child))
        with self.assertRaisesRegex(RuntimeError, "CONSOLE_START_TIMEOUT"):
            console.ensure_console(12345, timeout=0, probe=Mock(return_value=None), launch=Mock(return_value=child))
        child.kill.assert_not_called()

    def test_cancel_does_not_kill_shared_server(self):
        child = Mock()
        with self.assertRaises(KeyboardInterrupt):
            console.ensure_console(12345, probe=Mock(side_effect=[None, KeyboardInterrupt()]),
                                   launch=Mock(return_value=child))
        child.terminate.assert_not_called()
        child.kill.assert_not_called()

    def test_launch_detaches_without_install_or_output(self):
        with patch.object(console.Path, "is_file", return_value=True), patch.object(console.subprocess, "Popen") as popen:
            console.launch_console(12345)
        args, kwargs = popen.call_args
        self.assertIn("server.managed", args[0])
        self.assertTrue(kwargs["start_new_session"])
        self.assertEqual(kwargs["stdout"], console.subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], console.subprocess.DEVNULL)

    def test_missing_environment_never_installs(self):
        with patch.object(console.Path, "is_file", return_value=False), patch.object(console.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(RuntimeError, "CONSOLE_ENVIRONMENT_MISSING"):
                console.launch_console(12345)
        popen.assert_not_called()
