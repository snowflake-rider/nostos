"""Test the tester with local processes/temporary source copies, never devices."""
from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


runner = module("nostos_test_runner", ROOT / "tools/testing/run.py")
pins = module("nostos_pin_check", ROOT / "tools/testing/check_pins.py")


class PinTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nostos pin test ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for relative in ("firmware/stm32/Core", "firmware/stm32/MyApp", "firmware/esp32/main"):
            shutil.copytree(ROOT / relative, self.root / relative)
        for relative in ("firmware/stm32/nostos_stm32.ioc", "firmware/esp32/sdkconfig"):
            shutil.copy2(ROOT / relative, self.root / relative)

    def mutate(self, relative, old, new):
        path = self.root / relative
        text = path.read_text()
        self.assertTrue(old in text, f"fixture token missing: {old}")
        path.write_text(text.replace(old, new, 1))

    def result(self):
        return pins.inspect(self.root)

    def test_current_source_passes(self):
        result = self.result()
        self.assertEqual(result["status"], "PASS", result["issues"])
        self.assertEqual(result["stm32_pins"], 24)
        self.assertTrue(result["limitations"])

    def test_header_collision_fails(self):
        self.mutate("firmware/stm32/Core/Inc/main.h", "#define RGB_R_Pin GPIO_PIN_4", "#define RGB_R_Pin GPIO_PIN_0")
        self.assertEqual(self.result()["status"], "FAIL")

    def test_wrong_af_fails(self):
        self.mutate("firmware/stm32/Core/Src/stm32f4xx_hal_msp.c", "GPIO_AF7_USART1", "GPIO_AF5_SPI2")
        self.assertIn("AF/모드", " ".join(self.result()["issues"]))

    def test_missing_init_fails(self):
        self.mutate("firmware/stm32/Core/Src/main.c", "HAL_GPIO_Init(RGB_R_GPIO_Port, &GPIO_InitStruct);", "")
        self.assertEqual(self.result()["status"], "FAIL")

    def test_unknown_mask_blocks(self):
        self.mutate("firmware/stm32/Core/Inc/main.h", "#define RGB_R_Pin GPIO_PIN_4", "#define RGB_R_Pin (1 << 4)")
        with self.assertRaises(pins.Unsupported):
            self.result()

    def test_wrong_ioc_pull_fails(self):
        self.mutate("firmware/stm32/nostos_stm32.ioc", "GPIO_PULLDOWN", "GPIO_PULLUP")
        self.assertEqual(self.result()["status"], "FAIL")

    def test_extra_initializer_blocks(self):
        file = self.root / "firmware/stm32/MyApp/new_driver.c"
        file.write_text("void init(void) { HAL_GPIO_Init(GPIOA, &x); }")
        with self.assertRaises(pins.Unsupported):
            self.result()

    def test_conditional_initializer_blocks(self):
        self.mutate("firmware/stm32/Core/Src/main.c", "HAL_GPIO_Init(RGB_R_GPIO_Port", "if (ready) HAL_GPIO_Init(RGB_R_GPIO_Port")
        with self.assertRaises(pins.Unsupported):
            self.result()

    def test_uart_speed_mismatch_fails(self):
        self.mutate("firmware/stm32/Core/Src/main.c", "huart1.Init.BaudRate = 115200", "huart1.Init.BaudRate = 9600")
        self.assertEqual(self.result()["status"], "FAIL")

    def test_usb_pin_collision_fails(self):
        self.mutate("firmware/esp32/main/bridge_runtime.c", "#define UART_RX_GPIO 18", "#define UART_RX_GPIO 19")
        self.assertEqual(self.result()["status"], "FAIL")

    def test_wrong_target_fails(self):
        self.mutate("firmware/esp32/sdkconfig", 'CONFIG_IDF_TARGET="esp32s3"', 'CONFIG_IDF_TARGET="esp32"')
        self.assertEqual(self.result()["status"], "FAIL")

    def test_v2_selection_checked(self):
        self.mutate("firmware/esp32/sdkconfig", 'CONFIG_IDF_TARGET="esp32s3"', 'CONFIG_IDF_TARGET="esp32s3"\nCONFIG_NOSTOS_PROTOCOL_V2=y')
        self.assertEqual(self.result()["esp32_runtime"], "bridge_runtime_v2.c")
        self.mutate("firmware/esp32/main/bridge_runtime_v2.c", "DATA_UART,17,18,", "DATA_UART,17,20,")
        self.assertEqual(self.result()["status"], "FAIL")


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nostos runner test ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def execute(self, code, timeout=5):
        return runner.execute([sys.executable, "-c", code], self.root, os.environ.copy(), self.root / "process.log", timeout)

    def test_process_success_and_log(self):
        self.assertEqual(self.execute("print('details')")[0], "PASS")
        self.assertIn("details", (self.root / "process.log").read_text())

    def test_nonzero_never_passes(self):
        for value in (1, 2, 78):
            self.assertEqual(self.execute(f"raise SystemExit({value})")[0], "FAIL")

    def test_missing_executable_blocks(self):
        state, _ = runner.execute(["/nonexistent/nostos-test-tool"], self.root, os.environ.copy(), self.root / "missing.log", 1)
        self.assertEqual(state, "BLOCKED")

    def test_timeout_fails(self):
        self.assertEqual(self.execute("import time; time.sleep(10)", timeout=1)[0], "FAIL")

    def test_exit_codes_and_no_false_green(self):
        for statuses, expected in ((["PASS"], 0), (["READ"], 0), (["PASS", "FAIL"], 1),
                                   (["BLOCKED", "PASS"], 2), (["FAIL", "BLOCKED"], 1),
                                   (["CANCELLED", "NOT_RUN"], 130)):
            self.assertEqual(runner.exit_code([{"status": value} for value in statuses]), expected)

    def test_default_never_opens_usb_or_sends(self):
        self.assertNotIn("usb", runner.GROUPS["code"])
        self.assertFalse(any("mesh" in value or "send" in value for value in runner.GROUPS["code"]))

    def test_default_does_not_duplicate_message_protocol(self):
        self.assertNotIn("protocol", runner.GROUPS["code"])
        self.assertIn("message-protocol)", (ROOT / "tools/test-host.sh").read_text())

    def test_help_and_bad_option_offline(self):
        for args, code in ((["--help"], 0), (["--send"], 2), (["--timeout", "0"], 2)):
            result = subprocess.run(["bash", str(ROOT / "tests/run.sh"), *args], cwd=self.root, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, code)

    def test_continues_after_failure_and_saves_json(self):
        output = io.StringIO()
        def fake(key, out, *_):
            return ("FAIL" if key == "stm32-debug" else "PASS", "fixture", out / (key + ".log"))
        with patch.object(runner, "ROOT", self.root), patch.object(runner, "stage", side_effect=fake), redirect_stdout(output):
            self.assertEqual(runner.main(["stm32", "--json"]), 1)
        data = json.loads(output.getvalue())
        self.assertEqual([row["status"] for row in data["results"]], ["FAIL", "PASS"])
        self.assertEqual(json.loads((Path(data["artifacts"]) / "summary.json").read_text()), data)

    def test_cancel_stops_remaining_and_saves(self):
        with patch.object(runner, "ROOT", self.root), patch.object(runner, "stage", side_effect=KeyboardInterrupt) as call, redirect_stdout(io.StringIO()):
            self.assertEqual(runner.main(["stm32"]), 130)
        self.assertEqual(call.call_count, 1)
        data = json.loads(next(self.root.glob("build/test-results/*/summary.json")).read_text())
        self.assertEqual(data["results"][1]["status"], "NOT_RUN")

    def test_repeated_runs_preserve_artifacts(self):
        with patch.object(runner, "ROOT", self.root), patch.object(runner, "stage", return_value=("PASS", "fixture", self.root / "unused")), redirect_stdout(io.StringIO()):
            runner.main(["pins"])
            runner.main(["pins"])
        self.assertEqual(len(list(self.root.glob("build/test-results/*/summary.json"))), 2)

    def test_missing_build_tools_block_before_command(self):
        with patch.object(runner, "require", return_value="fixture missing gcc"), patch.object(runner, "execute") as command:
            state, _, _ = runner.stage("stm32-debug", self.root, {}, 1)
        self.assertEqual(state, "BLOCKED")
        command.assert_not_called()

    def test_usb_read_is_not_pass(self):
        (self.root / "usb.json").write_text(json.dumps({"result": "STATUS_READ_PARTIAL_CONFIG", "devices": [
            {"board": board, "status_result": "READ", "firmware_report": {"onoff_ready": "0"}}
            for board in ("D6", "76", "B6")]}))
        with patch.object(runner, "execute", return_value=("PASS", "done")) as command:
            state, _, _ = runner.stage("usb", self.root, {}, 1)
        self.assertEqual(state, "READ")
        self.assertNotIn("--send", command.call_args.args[0])

    def test_zero_exit_without_usb_evidence_blocks(self):
        with patch.object(runner, "execute", return_value=("PASS", "done")):
            state, _, _ = runner.stage("usb", self.root, {}, 1)
        self.assertEqual(state, "BLOCKED")

    def test_first_compiler_error_is_visible(self):
        path = self.root / "failure.log"
        path.write_text("noise\n/a/b/module.c:35:14: error: signature mismatch\nmore noise")
        self.assertEqual(runner.failure_hint(path), "module.c:35:14: error: signature mismatch")

    def test_unreviewed_esp_dependency_blocks_before_build(self):
        manifest = self.root / "firmware/esp32/main/idf_component.yml"
        manifest.parent.mkdir(parents=True)
        manifest.write_text("dependencies:\n  external/plugin: '*'\n")
        sdk = self.root / "idf"
        sdk.mkdir()
        (sdk / "export.sh").touch()
        with patch.object(runner, "ROOT", self.root), patch.object(runner, "execute") as command:
            state, note, _ = runner.stage("esp32", self.root, {"ESP_IDF_PATH": str(sdk)}, 1)
        self.assertEqual(state, "BLOCKED")
        self.assertIn("의존성 변경", note)
        command.assert_not_called()

    def test_stage_entrypoints_help_and_shell_syntax(self):
        for path in (ROOT / "tests/system").glob("*/run.sh"):
            with self.subTest(stage=path.parent.name):
                result = subprocess.run(["bash", "-n", str(path)], capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 0)
                result = subprocess.run(["bash", str(path), "--help"], cwd=path.parent, capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
