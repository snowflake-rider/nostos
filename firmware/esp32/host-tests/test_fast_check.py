"""Exercise the user-facing replay command, without touching hardware."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "fast_check.py"
ADDRESSES = {"D6": "0005", "76": "0003", "B6": "0004"}


def status(at, board, invalid=0, noop=0, errors=0, ready=1, valid=0):
    return [
        {"at": at, "board": board, "line":
         f"STATUS name=ESP32-L8-test primary=0x{ADDRESSES[board]} event_ready={ready} "
         "net=0x0000 app=0x0001 pub=0xc001 sub_C001=1"},
        {"at": at, "board": board, "line":
         f"QUEUE pending=0 capacity=32; uart_rx valid={valid} noop={noop} invalid={invalid} hw_errors={errors}"},
    ]


def snapshot(at, **kwargs):
    return [row for board in ADDRESSES for row in status(at, board, **kwargs)]


def end_snapshot(at, count=1, **kwargs):
    return [row for board in ADDRESSES
            for row in status(at, board, valid=count if board == "D6" else 0, **kwargs)]


def path(at=1.01, ident="13", source="0005"):
    return [
        {"at": at, "board": "D6", "line": f"UART_RX id=0x{ident} result=queued"},
        {"at": at + .01, "board": "D6", "line": f"MESH_TX id=0x{ident} source=0x{source} api=accepted age_ms=0"},
        {"at": at + .02, "board": "76", "line": f"MESH_RX source=0x{source} id=0x{ident} result=queued"},
        {"at": at + .03, "board": "B6", "line": f"MESH_RX source=0x{source} id=0x{ident} result=queued"},
    ]


def replay(rows):
    process = subprocess.run(
        [sys.executable, str(SCRIPT), "--replay", "-", "--json"],
        input="".join(json.dumps(row) + "\n" for row in rows),
        text=True, capture_output=True, timeout=5,
    )
    if process.returncode != 0:
        raise AssertionError(process.stderr or process.stdout)
    return [json.loads(line) for line in process.stdout.splitlines()]


class FastCheckTest(unittest.TestCase):
    def result(self, middle, ending=None):
        rows = snapshot(0) + [{"at": 1, "board": "STM32", "hex": "13"}]
        rows += middle + (end_snapshot(4.1) if ending is None else ending) + [{"at": 4.2}]
        return [r for r in replay(rows) if r["kind"] == "result"][0]

    def test_clean_path_is_observed_only_after_fresh_end_snapshot(self):
        rows = snapshot(0) + [{"at": 1, "board": "STM32", "hex": "13"}]
        rows += path() + end_snapshot(4.1) + [{"at": 4.2}]
        results = [row for row in replay(rows) if row["kind"] == "result"]
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["verdict"], "PASS_OBSERVED")
        self.assertEqual(results[0]["counts"], {
            "STM32": 1, "D6.UART_RX": 1, "D6.MESH_TX": 1,
            "76.MESH_RX": 1, "B6.MESH_RX": 1,
        })

    def test_configuration_change_during_trial_is_not_a_pass(self):
        rows = snapshot(0) + [{"at": 1, "board": "STM32", "hex": "13"}]
        rows += path()
        changed = status(2, "D6")[0]
        changed["line"] = changed["line"].replace("pub=0xc001", "pub=0xc002")
        rows += [changed] + end_snapshot(4.1) + [{"at": 4.2}]
        result = [r for r in replay(rows) if r["kind"] == "result"][0]
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")

    def test_missing_receiver_is_named(self):
        result = self.result(path()[:-1])
        self.assertEqual(result["verdict"], "INCOMPLETE")
        self.assertEqual(result["missing"], ["B6.MESH_RX"])

    def test_no_uart_arrival_names_the_broken_stages(self):
        result = self.result([], end_snapshot(4.1, count=0))
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")
        self.assertIn("D6.UART_RX", result["missing"])

    def test_noise_counters_block_pass_even_when_ids_match(self):
        for field in ("invalid", "noop", "errors"):
            with self.subTest(field=field):
                result = self.result(path(), end_snapshot(4.1, **{field: 1}))
                self.assertEqual(result["verdict"], "WARN")
                self.assertIn("D6_UART_NOISE", result["issues"])

    def test_wrong_id_is_not_counted_as_button(self):
        result = self.result(path(ident="11"))
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")
        self.assertEqual(result["counts"]["D6.UART_RX"], 0)

    def test_other_mesh_source_is_not_counted(self):
        result = self.result(path(source="0004"))
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")
        self.assertEqual(result["counts"]["76.MESH_RX"], 0)

    def test_duplicate_events_are_not_a_pass(self):
        result = self.result(path() + path(1.2), end_snapshot(4.1, count=2))
        self.assertEqual(result["verdict"], "WARN")
        self.assertIn("DUPLICATE_OR_EXTRA_EVENT", result["issues"])

    def test_mesh_api_failure_is_not_a_pass(self):
        rows = path()
        rows[1]["line"] = rows[1]["line"].replace("api=accepted", "api=failed")
        result = self.result(rows)
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")
        self.assertIn("D6.MESH_TX", result["missing"])

    def test_missing_end_snapshot_is_not_a_pass(self):
        result = self.result(path(), [])
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")
        self.assertIn("END_STATUS_MISSING", result["issues"])

    def test_restart_is_not_a_pass(self):
        result = self.result(path() + [{"at": 2, "board": "D6", "line": "[LAYER-8] BOOT_START"}])
        self.assertNotEqual(result["verdict"], "PASS_OBSERVED")
        self.assertIn("D6_RESTARTED", result["issues"])

    def test_no_button_or_orphan_logs_never_create_a_pass(self):
        rows = snapshot(0) + path(.1) + end_snapshot(.5) + [{"at": 12}]
        records = replay(rows)
        self.assertFalse(any(r["kind"] == "result" for r in records))
        self.assertTrue(any(r["kind"] == "idle" for r in records))

    def test_three_quick_presses_use_exact_counts(self):
        rows = snapshot(0) + [{"at": 1, "board": "STM32", "hex": "13 13 13"}]
        rows += path() + path(1.2) + path(1.4) + end_snapshot(4.1, count=3) + [{"at": 4.2}]
        result = [r for r in replay(rows) if r["kind"] == "result"][0]
        self.assertEqual(result["verdict"], "PASS_OBSERVED")
        self.assertTrue(all(count == 3 for count in result["counts"].values()))

    def test_second_trial_does_not_reuse_first_trial_events(self):
        rows = snapshot(0) + [{"at": 1, "board": "STM32", "hex": "13"}]
        rows += path() + end_snapshot(4.1) + [{"at": 4.2}]
        rows += [{"at": 5, "board": "STM32", "hex": "13"}] + path(5.01)
        rows += end_snapshot(8.1, count=2) + [{"at": 8.2}]
        results = [r for r in replay(rows) if r["kind"] == "result"]
        self.assertEqual([r["verdict"] for r in results], ["PASS_OBSERVED", "PASS_OBSERVED"])
        self.assertEqual([r["counts"]["STM32"] for r in results], [1, 1])


if __name__ == "__main__":
    unittest.main()
