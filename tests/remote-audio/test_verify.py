import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import verify


HERE = Path(__file__).resolve().parent


def line(stage, ident, *, source=None, accepted=True):
    fields = []
    if source is not None:
        fields.append(f"source=0x{source:04x}")
    fields.append(f"id=0x{ident:02x}")
    if stage.endswith("RX"):
        fields.append(f"result={'queued' if accepted else 'full'}")
    else:
        fields.append(f"api={'accepted' if accepted else 'failed'}")
    return f"I LAYER_8_UART: {stage} " + " ".join(fields)


def evidence(event="speed-up", ident=0x11):
    return {
        "event": event,
        "source": {
            "name": "A",
            "primary": "0x0005",
            "log": [line("UART_RX", ident), line("MESH_TX", ident, source=0x0005)],
        },
        "receiver": {
            "name": "B",
            "primary": "0x0006",
            "log": [line("MESH_RX", ident, source=0x0005), line("UART_TX", ident, source=0x0005)],
        },
        "receiver_stm32": {
            "rx_count_before": 8,
            "rx_count_after": 9,
            "remote_count_before": 3,
            "remote_count_after": 4,
            "last_received": f"0x{ident:02x}",
            "audio_status": 0,
            "audio_heard": True,
            "buzzer_heard": False,
            "rgb_changed": False,
        },
    }


class RemoteAudioVerifierTests(unittest.TestCase):
    def test_all_three_button_events_pass(self):
        for event, ident in verify.EVENT_IDS.items():
            with self.subTest(event=event):
                report = verify.verify(evidence(event, ident))
                self.assertEqual(report["verdict"], "PASS", report)
                self.assertEqual(report["problems"], [])

    def test_missing_mesh_stage_fails(self):
        sample = evidence()
        sample["receiver"]["log"] = [sample["receiver"]["log"][1]]
        report = verify.verify(sample)
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("MISSING_STAGE:B.MESH_RX", report["problems"])

    def test_duplicate_stage_fails(self):
        sample = evidence()
        sample["source"]["log"].append(sample["source"]["log"][0])
        report = verify.verify(sample)
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("DUPLICATE_STAGE:A.UART_RX:2", report["problems"])

    def test_uart_echo_loop_fails(self):
        sample = evidence()
        sample["receiver"]["log"].append(line("UART_RX", 0x11))
        report = verify.verify(sample)
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("UART_ECHO_LOOP:B:0x11", report["problems"])

    def test_fall_or_sos_during_button_trial_fails(self):
        for ident in (0x30, 0x31):
            with self.subTest(ident=ident):
                sample = evidence()
                sample["source"]["log"].append(line("UART_RX", ident))
                report = verify.verify(sample)
                self.assertEqual(report["verdict"], "FAIL")
                self.assertTrue(
                    any(problem.startswith("FORBIDDEN_SAFETY_EVENT") for problem in report["problems"])
                )

    def test_wrong_mesh_source_fails(self):
        sample = evidence()
        sample["receiver"]["log"][0] = line("MESH_RX", 0x11, source=0x0007)
        report = verify.verify(sample)
        self.assertEqual(report["verdict"], "FAIL")
        self.assertIn("WRONG_SOURCE:B.MESH_RX", report["problems"])

    def test_network_only_is_incomplete_not_pass(self):
        sample = evidence()
        del sample["receiver_stm32"]
        report = verify.verify(sample)
        self.assertEqual(report["verdict"], "INCOMPLETE")
        self.assertIn("STM32_EVIDENCE_MISSING", report["problems"])

    def test_audio_or_output_policy_failure_is_reported(self):
        cases = (
            ("audio_heard", False, "REMOTE_AUDIO_NOT_HEARD"),
            ("buzzer_heard", True, "BUTTON_TRIGGERED_BUZZER"),
            ("rgb_changed", True, "REMOTE_BUTTON_CHANGED_RGB"),
        )
        for field, value, problem in cases:
            with self.subTest(field=field):
                sample = evidence()
                sample["receiver_stm32"][field] = value
                report = verify.verify(sample)
                self.assertEqual(report["verdict"], "FAIL")
                self.assertIn(problem, report["problems"])

    def test_bad_schema_is_rejected(self):
        sample = evidence()
        sample["source"]["primary"] = "not-an-address"
        with self.assertRaises(verify.EvidenceError):
            verify.verify(sample)

    def test_cli_exit_codes_and_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.json"
            path.write_text(json.dumps(evidence()), encoding="utf-8")
            passed = subprocess.run(
                [sys.executable, str(HERE / "verify.py"), str(path), "--json"],
                text=True,
                capture_output=True,
                timeout=10,
            )
            self.assertEqual(passed.returncode, 0, passed.stderr)
            self.assertEqual(json.loads(passed.stdout)["verdict"], "PASS")

            incomplete = evidence()
            del incomplete["receiver_stm32"]
            path.write_text(json.dumps(incomplete), encoding="utf-8")
            failed = subprocess.run(
                [sys.executable, str(HERE / "verify.py"), str(path), "--json"],
                text=True,
                capture_output=True,
                timeout=10,
            )
            self.assertEqual(failed.returncode, 1, failed.stderr)
            self.assertEqual(json.loads(failed.stdout)["verdict"], "INCOMPLETE")


if __name__ == "__main__":
    unittest.main()
