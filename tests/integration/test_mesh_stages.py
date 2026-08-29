"""Exercise the numbered shell entrypoints with a local fake engine, never USB/API."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
STAGES = ("01-check", "02-delivery", "03-repeat", "04-relay")
ANSWERS = "fixed lab layout; TTL 7\nyes\nyes\nyes\nyes\n"
FAKE_ENGINE = r'''
import json
import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parent
args = sys.argv[1:]
with (root / "calls.jsonl").open("a", encoding="utf-8") as f:
    f.write(json.dumps(args) + "\n")
if args[0] == "check":
    sys.exit(int(os.environ.get("FIXTURE_CHECK_EXIT", "0")))
if args[0] == "run":
    phase = args[args.index("--phase") + 1] if "--phase" in args else "delivery"
    if phase == os.environ.get("FIXTURE_FAIL_PHASE"):
        sys.exit(2)
    if "--out" in args:
        out = Path(args[args.index("--out") + 1])
        out.mkdir(parents=True, exist_ok=False)
        report = {"stop": os.environ.get("FIXTURE_STOP", "COMPLETED"),
                  "completed": 20, "ambiguous": 0, "error": None}
        (out / "summary.json").write_text(json.dumps(report), encoding="utf-8")
if args[0] == "compare":
    code = int(os.environ.get("FIXTURE_COMPARE_EXIT", "0"))
    print(json.dumps({"verdict": "INCONCLUSIVE" if code else "RELAY_EFFECT_OBSERVED",
                      "fixture_only": True}))
    sys.exit(code)
'''


class MeshStageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nostos mesh stages ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / "tests/mesh", self.root / "tests/mesh")
        engine = self.root / "fake_engine.py"
        engine.write_text(FAKE_ENGINE, encoding="utf-8")
        runner = self.root / "tools/hardware/run_mesh_repeat.sh"
        runner.parent.mkdir(parents=True)
        runner.write_text(f"#!/usr/bin/env bash\nexec {shlex.quote(sys.executable)} "
                          f"{shlex.quote(str(engine))} \"$@\"\n", encoding="utf-8")

    def invoke(self, stage, *args, answers="", overrides=None, from_stage=False):
        directory = self.root / "tests/mesh" / stage
        return subprocess.run(
            ["bash", str(directory / "run.sh"), *args], input=answers,
            cwd=directory if from_stage else self.root, text=True, capture_output=True,
            env={**os.environ, **(overrides or {})}, timeout=15,
        )

    def calls(self):
        log = self.root / "calls.jsonl"
        return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []

    def test_all_help_is_offline(self):
        for stage in STAGES:
            with self.subTest(stage=stage):
                self.assertEqual(self.invoke(stage, "--help").returncode, 0)
        self.assertEqual(self.calls(), [])

    def test_send_is_explicit_and_unknown_options_are_rejected(self):
        for stage in STAGES:
            invalid = (("--send",), ("--unknown",)) if stage == "01-check" else (
                (), ("--unknown",), ("--send", "--count", "0"), ("--help", "--send"))
            for args in invalid:
                with self.subTest(stage=stage, args=args):
                    self.assertEqual(self.invoke(stage, *args).returncode, 2)
        self.assertEqual(self.calls(), [])

    def test_check_from_stage_directory_and_space_in_path(self):
        result = self.invoke("01-check", from_stage=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.calls(), [["check", "--source", "D6", "--peers", "76", "B6"]])

    def test_delivery_has_six_trials_and_fixed_roles(self):
        result = self.invoke("02-delivery", "--send")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.calls(), [["run", "--send", "--source", "D6", "--peers", "76", "B6",
                                        "--count", "6", "--interval", "5", "--window", "3"]])

    def test_repeat_keeps_existing_log_cap(self):
        result = self.invoke("03-repeat", "--send")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.calls(), [["run", "--send", "--source", "D6", "--peers", "76", "B6",
                                        "--count", "0", "--interval", "5", "--window", "3",
                                        "--max-log-mb", "50"]])

    def test_engine_errors_are_propagated(self):
        for stage in STAGES[:3]:
            args = () if stage == "01-check" else ("--send",)
            with self.subTest(stage=stage):
                result = self.invoke(stage, *args, overrides={"FIXTURE_CHECK_EXIT": "2",
                                                             "FIXTURE_FAIL_PHASE": "delivery"})
                self.assertEqual(result.returncode, 2, result.stderr)

    def test_relay_blocks_before_prompts_if_not_ready(self):
        result = self.invoke("04-relay", "--send", answers=ANSWERS,
                             overrides={"FIXTURE_CHECK_EXIT": "2"})
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual([c[0] for c in self.calls()], ["check"])
        self.assertFalse((self.root / "build").exists())

    def test_relay_empty_conditions_or_eof_never_sends(self):
        for answers in ("", " \t\n", "layout\n", "layout\nno\n", "layout\nyes\nno\n"):
            with self.subTest(answers=answers):
                result = self.invoke("04-relay", "--send", answers=answers)
                self.assertEqual(result.returncode, 2, result.stderr)
        self.assertTrue(all(c[0] == "check" for c in self.calls()))

    def test_relay_runs_off_on_off_then_compares_same_session(self):
        result = self.invoke("04-relay", "--send", answers=ANSWERS, from_stage=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = self.calls()
        self.assertEqual([c[0] for c in calls], ["check", "run", "run", "run", "compare"])
        self.assertEqual(calls[0], ["check", "--source", "D6", "--peers", "76", "--relay", "B6"])
        paths = []
        for call, phase, name in zip(calls[1:4], ("relay-off", "relay-on", "relay-off"),
                                     ("off-1", "on", "off-2")):
            out = Path(call[call.index("--out") + 1])
            paths.append(out)
            self.assertEqual(out.name, name)
            self.assertEqual(call, ["run", "--send", "--source", "D6", "--peers", "76", "--relay", "B6",
                                   "--phase", phase, "--confirm-isolated-topology", "--conditions",
                                   "fixed lab layout; TTL 7", "--count", "20", "--interval", "5",
                                   "--window", "3", "--out", str(out)])
        self.assertEqual(len({p.parent for p in paths}), 1)
        self.assertEqual(calls[4], ["compare", *(str(p / "summary.json") for p in paths)])
        comparison = json.loads((paths[0].parent / "comparison.json").read_text())
        self.assertEqual(comparison["verdict"], "RELAY_EFFECT_OBSERVED")

    def test_relay_incomplete_zero_exit_does_not_advance(self):
        for stop in ("INTERRUPTED", "DURATION", "LOG_LIMIT"):
            before = len(self.calls())
            with self.subTest(stop=stop):
                result = self.invoke("04-relay", "--send", answers=ANSWERS,
                                     overrides={"FIXTURE_STOP": stop})
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual([c[0] for c in self.calls()[before:]], ["check", "run"])

    def test_relay_phase_error_does_not_send_next_phase(self):
        result = self.invoke("04-relay", "--send", answers=ANSWERS,
                             overrides={"FIXTURE_FAIL_PHASE": "relay-on"})
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual([c[0] for c in self.calls()], ["check", "run", "run"])

    def test_relay_cancellation_between_phases_preserves_first_result(self):
        result = self.invoke("04-relay", "--send", answers="layout\nyes\nyes\nno\n")
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual([c[0] for c in self.calls()], ["check", "run"])
        self.assertEqual(len(list((self.root / "build").glob("**/summary.json"))), 1)

    def test_relay_inconclusive_is_saved_and_propagated(self):
        result = self.invoke("04-relay", "--send", answers=ANSWERS,
                             overrides={"FIXTURE_COMPARE_EXIT": "2"})
        self.assertEqual(result.returncode, 2, result.stderr)
        reports = list((self.root / "build").glob("**/comparison.json"))
        self.assertEqual(len(reports), 1)
        self.assertEqual(json.loads(reports[0].read_text())["verdict"], "INCONCLUSIVE")

    def test_repeated_sessions_do_not_overwrite_evidence(self):
        for _ in range(2):
            result = self.invoke("04-relay", "--send", answers=ANSWERS)
            self.assertEqual(result.returncode, 0, result.stderr)
        reports = list((self.root / "build").glob("**/comparison.json"))
        self.assertEqual(len(reports), 2)
        self.assertNotEqual(reports[0].parent, reports[1].parent)


if __name__ == "__main__":
    unittest.main()
