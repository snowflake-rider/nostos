"""Human summaries must not turn incomplete RF evidence into a PASS."""
from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("mesh_output", ROOT / "tools/hardware/mesh_repeat.py")
mesh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mesh)


class MeshOutputTests(unittest.TestCase):
    def test_preflight_human_never_says_pass_or_reconfigure(self):
        output = io.StringIO()
        with redirect_stdout(output):
            mesh.display_check(["D6: C000_SOURCE_NOT_READY"], {"D6": {"primary": "0x0005", "onoff_ready": "0"}})
        text = output.getvalue()
        self.assertIn("BLOCKED", text)
        self.assertIn("이미 확인했다면 재설정하지", text)
        self.assertNotIn("PASS", text)

    def test_ready_is_not_delivery(self):
        with redirect_stdout(io.StringIO()) as output:
            mesh.display_check([], {})
        self.assertIn("수신 성공 아님", output.getvalue())

    def test_run_outcomes(self):
        baseline = dict(stop="COMPLETED", error=None, completed=6, ambiguous=0, missing=0)
        for change, phase, expected in (
            ({}, "delivery", ("OBSERVED", 0)),
            ({"missing": 1}, "delivery", ("FAIL", 1)),
            ({"missing": 6}, "relay-off", ("OBSERVED", 0)),
            ({"completed": 0}, "delivery", ("INCONCLUSIVE", 2)),
            ({"ambiguous": 1}, "delivery", ("INCONCLUSIVE", 2)),
            ({"stop": "ERROR", "error": "fixture"}, "delivery", ("INCONCLUSIVE", 2)),
            ({"stop": "LOG_LIMIT"}, "delivery", ("INCONCLUSIVE", 2)),
            ({"stop": "DURATION"}, "delivery", ("INCONCLUSIVE", 2)),
            ({"stop": "INTERRUPTED"}, "delivery", ("CANCELLED", 130)),
        ):
            with self.subTest(change=change, phase=phase):
                self.assertEqual(mesh.run_result({**baseline, **change}, phase), expected)

    def test_advanced_check_keeps_json_contract(self):
        with patch.dict(os.environ, {"MESH_REPEAT_HUMAN": "0"}), patch.object(mesh.Console, "state", return_value={}), patch.object(mesh, "preflight", return_value=([], {})), redirect_stdout(io.StringIO()) as output:
            self.assertEqual(mesh.main(["check"]), 0)
        self.assertEqual(json.loads(output.getvalue())["verdict"], "READY_ONLY_NOT_DELIVERY")

    def test_stage_check_uses_human_output(self):
        with patch.dict(os.environ, {"MESH_REPEAT_HUMAN": "1"}), patch.object(mesh.Console, "state", return_value={}), patch.object(mesh, "preflight", return_value=(["D6: C000_SOURCE_NOT_READY"], {})), redirect_stdout(io.StringIO()) as output:
            self.assertEqual(mesh.main(["check"]), 2)
        self.assertTrue(output.getvalue().startswith("BLOCKED"))

    def test_send_still_requires_explicit_flag(self):
        args = mesh.parser().parse_args(["run"])
        with self.assertRaisesRegex(ValueError, "requires --send"):
            mesh.run(args, None)


if __name__ == "__main__":
    unittest.main()
