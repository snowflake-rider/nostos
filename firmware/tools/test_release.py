#!/usr/bin/env python3

from __future__ import annotations

import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import os
import shutil
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
FW_PATH = MODULE_PATH.with_name("fw")


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


class Stm32BuildToolDiscoveryTest(unittest.TestCase):
    TOOL_NAMES = ("cmake", "ninja", "arm-none-eabi-gcc", "arm-none-eabi-objcopy")

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.base = Path(self.temporary.name)
        self.root = self.base / "firmware"
        self.root.mkdir()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def executable(self, path: Path) -> Path:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/usr/bin/env sh\n", encoding="utf-8")
        path.chmod(0o755)
        return path.absolute()

    def tool_set(self, directory: Path) -> dict[str, Path]:
        return {name: self.executable(directory / name) for name in self.TOOL_NAMES}

    def test_prefers_path_and_emits_absolute_variant_commands(self) -> None:
        path_tools = self.tool_set(self.base / "path tools")
        self.tool_set(self.base / "data/nostos-toolchains/fallback/bin")
        with mock.patch.dict(
            os.environ,
            {"PATH": str(self.base / "path tools"), "XDG_DATA_HOME": str(self.base / "data")},
        ):
            tools = release.discover_stm32_build_tools(self.root)

        self.assertEqual(tools, path_tools)
        variant = {
            "buildDirectory": self.root / "stm32/build/Release-test",
            "artifact": self.root / "stm32/build/Release-test/nostos_stm32.bin",
            "buildPolicy": {
                "generator": "Ninja",
                "ssd1306Display": True,
                "mpu6050Sensor": False,
                "dht11Sensor": False,
            },
        }
        configure, build, convert = release.stm32_variant_commands(
            self.root, variant, tools
        )
        self.assertEqual(configure[0], str(path_tools["cmake"]))
        self.assertIn(f"-DCMAKE_MAKE_PROGRAM={path_tools['ninja']}", configure)
        self.assertIn(f"-DCMAKE_C_COMPILER={path_tools['arm-none-eabi-gcc']}", configure)
        self.assertIn(f"-DCMAKE_OBJCOPY={path_tools['arm-none-eabi-objcopy']}", configure)
        self.assertEqual(build[0], str(path_tools["cmake"]))
        self.assertEqual(convert[0], str(path_tools["arm-none-eabi-objcopy"]))
        self.assertTrue(all(Path(command[0]).is_absolute() for command in (configure, build, convert)))

    def test_falls_back_to_existing_release_metadata_and_cache(self) -> None:
        metadata_tools = self.tool_set(self.base / "metadata tools")
        build_directory = self.root / "stm32/build/Release"
        metadata = build_directory / "CMakeFiles/4.4.2/CMakeCCompiler.cmake"
        metadata.parent.mkdir(parents=True)
        metadata.write_text(
            f'set(CMAKE_C_COMPILER "{metadata_tools["arm-none-eabi-gcc"]}")\n',
            encoding="utf-8",
        )
        (build_directory / "CMakeCache.txt").write_text(
            f'CMAKE_COMMAND:INTERNAL={metadata_tools["cmake"]}\n'
            f'CMAKE_MAKE_PROGRAM:FILEPATH={metadata_tools["ninja"]}\n',
            encoding="utf-8",
        )
        with mock.patch.dict(
            os.environ,
            {"PATH": "", "XDG_DATA_HOME": str(self.base / "empty-data")},
        ):
            self.assertEqual(
                release.discover_stm32_build_tools(self.root), metadata_tools
            )

    def test_falls_back_to_xdg_nostos_toolchains(self) -> None:
        xdg_tools = self.tool_set(
            self.base / "data/nostos-toolchains/arm-sdk/bin"
        )
        with mock.patch.dict(
            os.environ,
            {"PATH": "", "XDG_DATA_HOME": str(self.base / "data")},
        ):
            self.assertEqual(release.discover_stm32_build_tools(self.root), xdg_tools)


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


class Stm32ReleaseVariantTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / "firmware"
        (self.root / "profiles").mkdir(parents=True)
        profile = json.loads(
            (MODULE_PATH.parent.parent / "profiles/release.json").read_text(
                encoding="utf-8"
            )
        )
        self.profile_file = self.root / "profiles/release.json"
        self.profile_file.write_text(json.dumps(profile), encoding="utf-8")
        self.tools = {
            "cmake": Path("/tools/cmake"),
            "ninja": Path("/tools/ninja"),
            "arm-none-eabi-gcc": Path("/tools/arm-none-eabi-gcc"),
            "arm-none-eabi-objcopy": Path("/tools/arm-none-eabi-objcopy"),
        }

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def profile(self) -> dict[str, object]:
        return json.loads(self.profile_file.read_text(encoding="utf-8"))

    def test_parses_three_unique_variants_and_keeps_legacy_alias(self) -> None:
        profile = release.validate_profile(self.root, self.profile_file)
        variants = release.stm32_variants(self.root, profile)
        self.assertEqual(
            [item["id"] for item in variants],
            ["node1-base", "node2-dht11", "node3-mpu6050"],
        )
        self.assertEqual(
            profile["targets"]["stm32"]["artifact"],
            profile["targets"]["stm32"]["variants"][0]["artifact"],
        )
        self.assertEqual(len({item["buildDirectory"] for item in variants}), 3)
        self.assertEqual(len({item["artifact"] for item in variants}), 3)

    def test_effective_commands_have_exact_sensor_flags(self) -> None:
        profile = release.validate_profile(self.root, self.profile_file)
        variants = release.stm32_variants(self.root, profile)
        configure = {
            item["id"]: release.stm32_variant_commands(self.root, item, self.tools)[0]
            for item in variants
        }
        self.assertIn("-DMPU6050_SENSOR=OFF", configure["node1-base"])
        self.assertIn("-DDHT11_SENSOR=OFF", configure["node1-base"])
        self.assertIn("-DMPU6050_SENSOR=OFF", configure["node2-dht11"])
        self.assertIn("-DDHT11_SENSOR=ON", configure["node2-dht11"])
        self.assertIn("-DMPU6050_SENSOR=ON", configure["node3-mpu6050"])
        self.assertIn("-DDHT11_SENSOR=OFF", configure["node3-mpu6050"])

    def test_artifact_receipt_and_manifest_identities_are_unique(self) -> None:
        profile = release.validate_profile(self.root, self.profile_file)
        artifacts = [
            item
            for item in release.required_artifacts(self.root, profile)
            if item["target"] == "stm32"
        ]
        self.assertEqual(len({item["relative"] for item in artifacts}), 3)
        receipt_paths = {
            release.build_receipt_path(self.root, "stm32", item["variant"])
            for item in artifacts
        }
        self.assertEqual(len(receipt_paths), 3)
        records = [
            {
                "target": "stm32",
                "role": "application",
                "variant": item["variant"],
                "label": item["label"],
                "path": item["relative"].as_posix(),
                "offset": "0x08000000",
            }
            for item in artifacts
        ]
        entries = release.stm32_manifest_entries(self.root, profile, records)
        self.assertEqual(len({entry["variant"] for entry in entries}), 3)
        self.assertEqual(len({entry["label"] for entry in entries}), 3)
        selected = release.select_stm32_flash_entry(
            entries,
            {
                "hardwareProfile": {
                    "ssd1306Display": True,
                    "mpu6050Sensor": False,
                    "dht11Sensor": True,
                }
            },
        )
        self.assertEqual(selected["variant"], "node2-dht11")
        self.assertEqual(selected["label"], "Node 2 DHT11")

    def test_rejects_duplicate_variant_artifact(self) -> None:
        profile = self.profile()
        variants = profile["targets"]["stm32"]["variants"]
        variants[1]["artifact"] = variants[0]["artifact"]
        variants[1]["buildDirectory"] = variants[0]["buildDirectory"]
        self.profile_file.write_text(json.dumps(profile), encoding="utf-8")
        with self.assertRaisesRegex(release.ReleaseError, "unique"):
            release.validate_profile(self.root, self.profile_file)

    def test_profile_without_variants_keeps_single_target_contract(self) -> None:
        profile = self.profile()
        del profile["targets"]["stm32"]["variants"]
        self.profile_file.write_text(json.dumps(profile), encoding="utf-8")
        validated = release.validate_profile(self.root, self.profile_file)
        variants = release.stm32_variants(self.root, validated)
        self.assertEqual(len(variants), 1)
        self.assertTrue(variants[0]["legacy"])
        self.assertEqual(
            release.build_receipt_path(self.root, "stm32").name, "stm32.json"
        )


class ReleaseVerificationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.release_directory = Path(self.temporary.name) / "nostos-v1.2.3"
        self.release_directory.mkdir()
        profile = json.loads(
            (MODULE_PATH.parent.parent / "profiles/release.json").read_text(
                encoding="utf-8"
            )
        )
        profile["status"] = "approved"
        profile_path = self.release_directory / "profile/release.json"
        profile_path.parent.mkdir(parents=True)
        profile_path.write_text(
            json.dumps(profile, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

        records: list[dict[str, object]] = []
        for variant in release.stm32_variants(self.release_directory, profile):
            relative = Path("stm32") / variant["id"] / "nostos_stm32.bin"
            image = self.release_directory / relative
            image.parent.mkdir(parents=True)
            image.write_bytes(f"stm32-{variant['id']}".encode())
            records.append(
                release.artifact_record(
                    "stm32",
                    "application",
                    relative,
                    image,
                    "0x08000000",
                    variant=variant["id"],
                    label=variant["label"],
                )
            )

        esp32_artifacts = (
            ("bootloader", Path("esp32/bootloader/bootloader.bin"), "0x0"),
            (
                "partition-table",
                Path("esp32/partition_table/partition-table.bin"),
                "0x8000",
            ),
            ("application", Path("esp32/nostos_esp32.bin"), "0x10000"),
            ("flasher-arguments", Path("esp32/flasher_args.json"), None),
        )
        for role, relative, offset in esp32_artifacts:
            image = self.release_directory / relative
            image.parent.mkdir(parents=True, exist_ok=True)
            image.write_bytes(f"esp32-{role}".encode())
            records.append(
                release.artifact_record("esp32", role, relative, image, offset)
            )
        profile_record = release.artifact_record(
            "bundle", "release-profile", Path("profile/release.json"), profile_path
        )
        records.append(profile_record)
        partition_record = next(
            item
            for item in records
            if item["target"] == "esp32" and item["role"] == "partition-table"
        )
        esp32_application = next(
            item
            for item in records
            if item["target"] == "esp32" and item["role"] == "application"
        )
        self.manifest_path = self.release_directory / "manifest.json"
        self.manifest = {
            "schemaVersion": 1,
            "release": {
                "id": "nostos-v1.2.3",
                "version": "1.2.3",
                "firmwareVersion": "v1.2.3",
                "gitCommit": "a" * 40,
                "gitTag": "nostos-v1.2.3",
                "profileStatus": "approved",
                "profileSha256": profile_record["sha256"],
            },
            "protocols": json.loads(json.dumps(profile["protocols"])),
            "artifacts": records,
            "flashPlan": {
                "stm32": {
                    "chip": "STM32F411xC_xE",
                    "mode": "application-only",
                    "entries": release.stm32_manifest_entries(
                        self.release_directory, profile, records
                    ),
                },
                "esp32": {
                    "chip": "esp32s3",
                    "mode": "application-only",
                    "partitionLayout": "single-factory-v1",
                    "partitionTableSha256": partition_record["sha256"],
                    "entries": [
                        {
                            "role": "application",
                            "path": esp32_application["path"],
                            "offset": esp32_application["offset"],
                        }
                    ],
                },
            },
        }
        self.write_manifest()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_manifest(self) -> None:
        self.manifest_path.write_text(
            json.dumps(self.manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def test_valid_release_protocols_match_packaged_profile(self) -> None:
        release.verify_release(self.release_directory)

    def test_rejects_tampered_manifest_protocol_contract(self) -> None:
        self.manifest["protocols"]["mesh"]["definitionRevision"] += 1
        self.write_manifest()
        with self.assertRaisesRegex(release.ReleaseError, "exactly match"):
            release.verify_release(self.release_directory)


class FwDoctorLayoutTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / "repo"
        for directory in (
            "firmware/stm32",
            "firmware/esp32",
            "firmware/protocol",
            "firmware/profiles",
            "firmware/inventory",
            "firmware/tools",
            "releases/baselines",
            "apps",
            "docs",
        ):
            (self.root / directory).mkdir(parents=True, exist_ok=True)
        for relative in (
            ".gitignore",
            "AGENTS.md",
            "CONTEXT.md",
            "DEVICES.md",
            "PINS.md",
            "README.md",
            "STRUCTURE.md",
            "firmware/VERSION",
            "firmware/README.md",
            "firmware/build.sh",
            "firmware/test-host.sh",
            "releases/README.md",
            "releases/index.json",
        ):
            (self.root / relative).write_text("placeholder\n", encoding="utf-8")
        shutil.copy2(FW_PATH, self.root / "firmware/tools/fw")
        shutil.copy2(MODULE_PATH, self.root / "firmware/tools/release.py")
        shutil.copy2(
            MODULE_PATH.parent.parent / "profiles/release.json",
            self.root / "firmware/profiles/release.json",
        )
        shutil.copy2(
            MODULE_PATH.parent.parent / "inventory/boards.example.json",
            self.root / "firmware/inventory/boards.example.json",
        )
        (self.root / "docs/tracked.md").write_text("tracked\n", encoding="utf-8")
        (self.root / "unrelated-root-item.txt").write_text("report only\n", encoding="utf-8")
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(
            ["git", "-C", str(self.root), "add", "docs/tracked.md"], check=True
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_tracked_docs_are_not_local_exceptions_and_root_extras_are_report_only(
        self,
    ) -> None:
        result = subprocess.run(
            ["bash", str(self.root / "firmware/tools/fw"), "doctor"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("docs/tracked.md", result.stderr)
        self.assertIn("unexpected root entry (report only)", result.stderr)


class InventoryExampleTest(unittest.TestCase):
    def test_example_contains_three_stm32_esp32_pairs(self) -> None:
        inventory = json.loads(
            (MODULE_PATH.parent.parent / "inventory/boards.example.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(inventory.get("schemaVersion"), 1)
        devices = inventory.get("devices")
        self.assertIsInstance(devices, list)
        pairs = {
            node: sorted(
                item.get("target")
                for item in devices
                if isinstance(item, dict) and item.get("id") == node
            )
            for node in ("node1", "node2", "node3")
        }
        self.assertEqual(
            pairs,
            {
                "node1": ["esp32", "stm32"],
                "node2": ["esp32", "stm32"],
                "node3": ["esp32", "stm32"],
            },
        )
        self.assertEqual(len(devices), 6)
        self.assertTrue(all(item.get("enabled") is False for item in devices))


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
