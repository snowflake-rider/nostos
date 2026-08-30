#!/usr/bin/env python3

from __future__ import annotations

import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock


MODULE_PATH = Path(__file__).with_name("release.py")
SPEC = importlib.util.spec_from_file_location("nostos_release", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class Stm32CompilerMetadataTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / "firmware"
        self.metadata = (
            self.root / "stm32/build/Release/CMakeFiles/4.4.2/CMakeCCompiler.cmake"
        )
        self.metadata.parent.mkdir(parents=True)
        self.compiler = self.root / "tool chain/bin/arm-none-eabi-gcc"
        self.compiler.parent.mkdir(parents=True)
        self.compiler.write_text("#!/usr/bin/env sh\n", encoding="utf-8")
        self.compiler.chmod(0o755)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_metadata(self, compiler: Path | str) -> None:
        self.metadata.write_text(
            f'set(CMAKE_C_COMPILER "{compiler}")\n'
            'set(CMAKE_C_COMPILER_ID "GNU")\n',
            encoding="utf-8",
        )

    def test_resolves_executable_from_cmake_build_metadata(self) -> None:
        self.write_metadata(self.compiler)
        with mock.patch.dict(os.environ, {"PATH": ""}):
            self.assertEqual(release.stm32_build_compiler(self.root), self.compiler.resolve())

    def test_rejects_missing_compiler_executable(self) -> None:
        self.write_metadata(self.root / "missing/arm-none-eabi-gcc")
        with self.assertRaisesRegex(release.ReleaseError, "missing or not executable"):
            release.stm32_build_compiler(self.root)

    def test_rejects_ambiguous_compiler_metadata(self) -> None:
        self.write_metadata(self.compiler)
        second_compiler = self.root / "other/bin/arm-none-eabi-gcc"
        second_compiler.parent.mkdir(parents=True)
        second_compiler.write_text("#!/usr/bin/env sh\n", encoding="utf-8")
        second_compiler.chmod(0o755)
        second_metadata = (
            self.root / "stm32/build/Release/CMakeFiles/4.5.0/CMakeCCompiler.cmake"
        )
        second_metadata.parent.mkdir(parents=True)
        second_metadata.write_text(
            f'set(CMAKE_C_COMPILER "{second_compiler}")\n', encoding="utf-8"
        )
        with self.assertRaisesRegex(release.ReleaseError, "ambiguous"):
            release.stm32_build_compiler(self.root)


class Esp32IdfMetadataTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / "firmware"
        self.cache = self.root / "esp32/build/CMakeCache.txt"
        self.cache.parent.mkdir(parents=True)
        self.python = self.root / "python env/bin/python"
        self.python.parent.mkdir(parents=True)
        base_python = self.python.with_name("python3.14")
        base_python.write_text("#!/usr/bin/env sh\n", encoding="utf-8")
        base_python.chmod(0o755)
        self.python.symlink_to(base_python.name)
        self.idf_root = self.root / "esp idf"
        self.idf_script = self.idf_root / "tools/idf.py"
        self.idf_script.parent.mkdir(parents=True)
        self.idf_script.write_text("#!/usr/bin/env python\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_cache(self, python: Path | str) -> None:
        self.cache.write_text(f"PYTHON:UNINITIALIZED={python}\n", encoding="utf-8")

    def test_resolves_python_and_idf_script_from_build_metadata(self) -> None:
        self.write_cache(self.python)
        description = {"idf_path": str(self.idf_root)}
        with mock.patch.dict(os.environ, {"PATH": ""}):
            self.assertEqual(
                release.esp32_build_idf_command(self.root, description),
                [str(self.python.absolute()), str(self.idf_script.resolve())],
            )

    def test_rejects_missing_build_python(self) -> None:
        self.write_cache(self.root / "missing/python")
        with self.assertRaisesRegex(release.ReleaseError, "not an executable"):
            release.esp32_build_idf_command(
                self.root, {"idf_path": str(self.idf_root)}
            )

    @mock.patch.object(release.subprocess, "run")
    def test_reads_idf_version_after_environment_notices(self, run: mock.Mock) -> None:
        run.return_value = subprocess.CompletedProcess(
            [],
            0,
            "Setting IDF_PATH environment variable\n"
            "WARNING: IDF_PYTHON_ENV_PATH is missing\n"
            "ESP-IDF v5.5.5\n",
        )
        self.assertEqual(
            release.command_version(
                [str(self.python), str(self.idf_script)], output_prefix="ESP-IDF "
            ),
            "ESP-IDF v5.5.5",
        )


class DevFlashTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / "firmware"
        (self.root / "profiles").mkdir(parents=True)
        (self.root / "stm32/build/Release").mkdir(parents=True)
        (self.root / "esp32/build/partition_table").mkdir(parents=True)
        (self.root / "inventory").mkdir(parents=True)
        (self.root / "out/releases").mkdir(parents=True)

        profile = json.loads(
            (MODULE_PATH.parent.parent / "profiles/release.json").read_text(encoding="utf-8")
        )
        self.profile = self.root / "profiles/release.json"
        self.profile.write_text(json.dumps(profile), encoding="utf-8")
        (self.root / "VERSION").write_text("1.0.0\n", encoding="utf-8")
        (self.root / "build.sh").write_text("#!/usr/bin/env bash\n", encoding="utf-8")
        self.stm32_image = self.root / "stm32/build/Release/nostos_stm32.bin"
        self.stm32_image.write_bytes(b"stm32-app")
        self.esp32_image = self.root / "esp32/build/nostos_esp32.bin"
        self.esp32_image.write_bytes(b"esp32-app")
        self.partition = self.root / "esp32/build/partition_table/partition-table.bin"
        self.partition.write_bytes(b"partition-v1")
        partition_sha = hashlib.sha256(self.partition.read_bytes()).hexdigest()
        inventory = {
            "schemaVersion": 1,
            "devices": [
                {
                    "id": "rider-1",
                    "target": "stm32",
                    "transport": "stlink",
                    "serial": "TEST_STLINK_SERIAL",
                    "hardwareProfile": {
                        "ssd1306Display": True,
                        "mpu6050Sensor": False,
                        "dht11Sensor": True,
                    },
                    "enabled": True,
                },
                {
                    "id": "rider-1",
                    "target": "esp32",
                    "transport": "serial",
                    "port": "/dev/test-esp32",
                    "partitionLayout": "single-factory-v1",
                    "partitionTableSha256": partition_sha,
                    "enabled": True,
                },
            ],
        }
        self.inventory = self.root / "inventory/boards.local.json"
        self.inventory.write_text(json.dumps(inventory), encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def args(self, target: str, *, execute: bool) -> argparse.Namespace:
        return argparse.Namespace(
            firmware_root=self.root,
            profile=self.profile,
            inventory=self.inventory,
            target=target,
            node="rider-1",
            dry_run=not execute,
            execute=execute,
        )

    def invoke(self, args: argparse.Namespace) -> None:
        with contextlib.redirect_stdout(io.StringIO()):
            release.dev_flash(args)

    @mock.patch.object(release.subprocess, "run")
    def test_dry_run_never_builds_or_touches_hardware(self, run: mock.Mock) -> None:
        self.invoke(self.args("stm32", execute=False))
        run.assert_not_called()

    @mock.patch.object(release.shutil, "which", return_value="/fake/st-flash")
    @mock.patch.object(release.subprocess, "run")
    def test_stm32_execute_builds_and_writes_one_node_without_readback(
        self, run: mock.Mock, _which: mock.Mock
    ) -> None:
        run.return_value = subprocess.CompletedProcess([], 0)
        self.invoke(self.args("stm32", execute=True))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertEqual(commands[0], ["bash", str((self.root / "build.sh").resolve()), "stm32"])
        build_environment = run.call_args_list[0].kwargs["env"]
        self.assertEqual(build_environment["NOSTOS_STM32_SSD1306_DISPLAY"], "ON")
        self.assertEqual(build_environment["NOSTOS_STM32_MPU6050_SENSOR"], "OFF")
        self.assertEqual(build_environment["NOSTOS_STM32_DHT11_SENSOR"], "ON")
        self.assertEqual(
            commands[1],
            [
                "/fake/st-flash",
                "--serial",
                "TEST_STLINK_SERIAL",
                "--reset",
                "write",
                str(self.stm32_image.resolve()),
                "0x08000000",
            ],
        )
        self.assertNotIn("read", commands[1])

    @mock.patch.object(release.shutil, "which", return_value="/fake/esptool.py")
    @mock.patch.object(release.subprocess, "run")
    def test_esp32_execute_writes_application_only(
        self, run: mock.Mock, _which: mock.Mock
    ) -> None:
        run.return_value = subprocess.CompletedProcess([], 0)
        self.invoke(self.args("esp32", execute=True))
        command = run.call_args_list[1].args[0]
        self.assertIn("write_flash", command)
        self.assertIn("0x10000", command)
        self.assertIn(str(self.esp32_image.resolve()), command)
        self.assertNotIn(str(self.partition.resolve()), command)
        self.assertNotIn("erase_flash", command)

    @mock.patch.object(release.subprocess, "run")
    def test_esp32_partition_mismatch_fails_before_build(self, run: mock.Mock) -> None:
        data = json.loads(self.inventory.read_text(encoding="utf-8"))
        data["devices"][1]["partitionTableSha256"] = "0" * 64
        self.inventory.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(release.ReleaseError, "partitionTableSha256"):
            self.invoke(self.args("esp32", execute=True))
        run.assert_not_called()

    @mock.patch.object(release.subprocess, "run")
    def test_missing_node_fails_before_build(self, run: mock.Mock) -> None:
        args = self.args("stm32", execute=True)
        args.node = "missing"
        with self.assertRaisesRegex(release.ReleaseError, "exactly one"):
            self.invoke(args)
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
