#!/usr/bin/env python3
"""Build, inspect, and verify NOSTOS firmware release and development flows."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


SEMVER_RE = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)")
REQUIRED_PROFILE_PATHS = (
    ("schemaVersion",),
    ("profile",),
    ("status",),
    ("versionFile",),
    ("targets", "stm32", "chip"),
    ("targets", "stm32", "flashAddress"),
    ("targets", "stm32", "buildPolicy", "generator"),
    ("targets", "stm32", "buildPolicy", "protocolV2"),
    ("targets", "stm32", "buildPolicy", "buttonOutputTest"),
    ("targets", "stm32", "buildPolicy", "ssd1306Display"),
    ("targets", "stm32", "buildPolicy", "mpu6050Sensor"),
    ("targets", "stm32", "buildPolicy", "dht11Sensor"),
    ("targets", "stm32", "artifact"),
    ("targets", "esp32", "chip"),
    ("targets", "esp32", "espIdf"),
    ("targets", "esp32", "defaultFlashMode"),
    ("targets", "esp32", "partitionLayout"),
    ("targets", "esp32", "flashOffsets", "bootloader"),
    ("targets", "esp32", "flashOffsets", "partitionTable"),
    ("targets", "esp32", "flashOffsets", "application"),
    ("targets", "esp32", "buildPolicy", "protocolV2"),
    ("targets", "esp32", "buildPolicy", "flashMode"),
    ("targets", "esp32", "buildPolicy", "flashSize"),
    ("targets", "esp32", "buildPolicy", "flashFrequency"),
    ("targets", "esp32", "artifacts", "bootloader"),
    ("targets", "esp32", "artifacts", "partitionTable"),
    ("targets", "esp32", "artifacts", "application"),
    ("targets", "esp32", "artifacts", "flasherArgs"),
    ("protocols", "uart"),
    ("protocols", "mesh"),
    ("package", "outputDirectory"),
)


class ReleaseError(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ReleaseError(f"missing JSON file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ReleaseError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise ReleaseError(f"expected a JSON object: {path}")
    return data


def nested(data: dict[str, Any], keys: tuple[str, ...]) -> Any:
    value: Any = data
    for key in keys:
        if not isinstance(value, dict) or key not in value:
            raise ReleaseError(f"release profile is missing {'.'.join(keys)}")
        value = value[key]
    return value


def within(base: Path, candidate: Path) -> bool:
    try:
        candidate.relative_to(base)
    except ValueError:
        return False
    return True


def profile_path(firmware_root: Path, value: Any, label: str) -> Path:
    if not isinstance(value, str) or not value or Path(value).is_absolute():
        raise ReleaseError(f"{label} must be a non-empty relative path")
    lexical = (firmware_root / value).absolute()
    resolved = lexical.resolve()
    if not within(firmware_root, resolved):
        raise ReleaseError(f"{label} escapes the firmware root: {value}")
    return lexical


def canonical_profile_path(firmware_root: Path, requested: Path) -> Path:
    expected_lexical = firmware_root / "profiles/release.json"
    requested_lexical = requested.expanduser()
    expected = expected_lexical.resolve()
    candidate = requested_lexical.resolve()
    if candidate != expected:
        raise ReleaseError(f"operation requires the canonical profile: {expected}")
    if (
        not expected_lexical.is_file()
        or expected_lexical.is_symlink()
        or requested_lexical.is_symlink()
    ):
        raise ReleaseError("canonical release profile must be a regular, non-symlink file")
    return expected_lexical


def validate_profile(firmware_root: Path, profile_file: Path) -> dict[str, Any]:
    firmware_root = firmware_root.resolve()
    profile = load_json(profile_file)
    for keys in REQUIRED_PROFILE_PATHS:
        nested(profile, keys)
    if profile["schemaVersion"] != 1:
        raise ReleaseError("unsupported release profile schemaVersion (expected 1)")
    if profile["profile"] != "release":
        raise ReleaseError("release profile 'profile' must be 'release'")
    if profile["status"] not in {"draft", "approved"}:
        raise ReleaseError("release profile status must be 'draft' or 'approved'")
    if nested(profile, ("targets", "stm32", "chip")) != "STM32F411xC_xE":
        raise ReleaseError("release profile STM32 chip must be 'STM32F411xC_xE'")
    if nested(profile, ("targets", "stm32", "buildPolicy", "generator")) != "Ninja":
        raise ReleaseError("release profile STM32 generator must be 'Ninja'")
    for option in (
        "protocolV2",
        "buttonOutputTest",
        "ssd1306Display",
        "mpu6050Sensor",
        "dht11Sensor",
    ):
        if not isinstance(nested(profile, ("targets", "stm32", "buildPolicy", option)), bool):
            raise ReleaseError(f"release profile STM32 buildPolicy.{option} must be boolean")
    if nested(profile, ("targets", "esp32", "chip")) != "esp32s3":
        raise ReleaseError("release profile ESP32 chip must be 'esp32s3'")
    if nested(profile, ("targets", "esp32", "espIdf")) != "v5.5.5":
        raise ReleaseError("release profile ESP-IDF must be 'v5.5.5'")
    if nested(profile, ("targets", "esp32", "defaultFlashMode")) != "application-only":
        raise ReleaseError("release profile ESP32 defaultFlashMode must be 'application-only'")
    partition_layout = nested(profile, ("targets", "esp32", "partitionLayout"))
    if not isinstance(partition_layout, str) or not partition_layout:
        raise ReleaseError("release profile ESP32 partitionLayout must be a non-empty string")
    for role in ("bootloader", "partitionTable", "application"):
        canonical_offset(
            nested(profile, ("targets", "esp32", "flashOffsets", role)),
            f"ESP32 flashOffsets.{role}",
        )
    if not isinstance(nested(profile, ("targets", "esp32", "buildPolicy", "protocolV2")), bool):
        raise ReleaseError("release profile ESP32 buildPolicy.protocolV2 must be boolean")
    for option in ("flashMode", "flashSize", "flashFrequency"):
        if not isinstance(nested(profile, ("targets", "esp32", "buildPolicy", option)), str):
            raise ReleaseError(f"release profile ESP32 buildPolicy.{option} must be a string")
    canonical_offset(nested(profile, ("targets", "stm32", "flashAddress")), "STM32 flashAddress")
    for protocol_name in ("uart", "mesh"):
        protocol = nested(profile, ("protocols", protocol_name))
        if not isinstance(protocol, dict) or not isinstance(protocol.get("version"), int):
            raise ReleaseError(f"protocols.{protocol_name}.version must be an integer")
        if protocol["version"] < 1:
            raise ReleaseError(f"protocols.{protocol_name}.version must be positive")

    path_keys = (
        ("versionFile",),
        ("targets", "stm32", "artifact"),
        ("targets", "esp32", "artifacts", "bootloader"),
        ("targets", "esp32", "artifacts", "partitionTable"),
        ("targets", "esp32", "artifacts", "application"),
        ("targets", "esp32", "artifacts", "flasherArgs"),
        ("package", "outputDirectory"),
    )
    for keys in path_keys:
        profile_path(firmware_root, nested(profile, keys), ".".join(keys))

    output_path = profile_path(
        firmware_root, nested(profile, ("package", "outputDirectory")), "package.outputDirectory"
    )
    expected_output = (firmware_root / "out" / "releases").resolve()
    if output_path != expected_output:
        raise ReleaseError("package.outputDirectory must be 'out/releases'")
    return profile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_offset(value: Any, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"0x[0-9a-fA-F]+", value):
        raise ReleaseError(f"{label} must be a hexadecimal address such as 0x10000")
    return "0x" + value[2:].lower()


def git(repo_root: Path, *args: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(repo_root), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except FileNotFoundError as exc:
        raise ReleaseError("git is required to create an official package") from exc
    except subprocess.CalledProcessError as exc:
        detail = exc.stderr.strip() or exc.stdout.strip() or "git command failed"
        raise ReleaseError(detail) from exc
    return result.stdout.strip()


def require_clean_git(repo_root: Path) -> str:
    if git(repo_root, "rev-parse", "--show-toplevel") != str(repo_root.resolve()):
        raise ReleaseError(f"repo root mismatch: {repo_root}")
    dirty = git(repo_root, "status", "--porcelain=v1", "--untracked-files=normal")
    if dirty:
        preview = "\n".join(dirty.splitlines()[:8])
        suffix = "\n..." if len(dirty.splitlines()) > 8 else ""
        raise ReleaseError(
            "official packages require a clean worktree; commit or remove changes first:\n"
            f"{preview}{suffix}"
        )
    return git(repo_root, "rev-parse", "HEAD")


def git_state(repo_root: Path) -> tuple[str, bool]:
    if git(repo_root, "rev-parse", "--show-toplevel") != str(repo_root.resolve()):
        raise ReleaseError(f"repo root mismatch: {repo_root}")
    commit = git(repo_root, "rev-parse", "HEAD")
    dirty = git(repo_root, "status", "--porcelain=v1", "--untracked-files=normal")
    return commit, not bool(dirty)


def require_annotated_release_tag(repo_root: Path, release_id: str, commit: str) -> None:
    reference = f"refs/tags/{release_id}"
    try:
        object_type = git(repo_root, "cat-file", "-t", reference)
        tagged_commit = git(repo_root, "rev-parse", f"{reference}^{{commit}}")
    except ReleaseError as exc:
        raise ReleaseError(f"missing annotated release tag {release_id}") from exc
    if object_type != "tag":
        raise ReleaseError(f"release tag {release_id} must be annotated, not lightweight")
    if tagged_commit != commit:
        raise ReleaseError(
            f"release tag {release_id} points to {tagged_commit}, expected current HEAD {commit}"
        )


def semver(value: str) -> str:
    if not SEMVER_RE.fullmatch(value):
        raise ReleaseError("version must be strict X.Y.Z SemVer without a leading 'v'")
    return value


def required_artifacts(firmware_root: Path, profile: dict[str, Any]) -> list[dict[str, Any]]:
    stm32 = profile_path(
        firmware_root,
        nested(profile, ("targets", "stm32", "artifact")),
        "targets.stm32.artifact",
    )
    esp = nested(profile, ("targets", "esp32", "artifacts"))
    result: list[dict[str, Any]] = [
        {
            "target": "stm32",
            "role": "application",
            "source": stm32,
            "relative": Path("stm32/nostos_stm32.bin"),
        },
        {
            "target": "esp32",
            "role": "bootloader",
            "source": profile_path(
                firmware_root, esp["bootloader"], "targets.esp32.artifacts.bootloader"
            ),
            "relative": Path("esp32/bootloader/bootloader.bin"),
        },
        {
            "target": "esp32",
            "role": "partition-table",
            "source": profile_path(
                firmware_root, esp["partitionTable"], "targets.esp32.artifacts.partitionTable"
            ),
            "relative": Path("esp32/partition_table/partition-table.bin"),
        },
        {
            "target": "esp32",
            "role": "application",
            "source": profile_path(
                firmware_root, esp["application"], "targets.esp32.artifacts.application"
            ),
            "relative": Path("esp32/nostos_esp32.bin"),
        },
        {
            "target": "esp32",
            "role": "flasher-arguments",
            "source": profile_path(
                firmware_root, esp["flasherArgs"], "targets.esp32.artifacts.flasherArgs"
            ),
            "relative": Path("esp32/flasher_args.json"),
        },
    ]

    esp_build_lexical = firmware_root / "esp32" / "build"
    if esp_build_lexical.is_symlink():
        raise ReleaseError("ESP32 build directory must not be a symlink")
    esp_build = esp_build_lexical.resolve()
    for relative in (
        "flash_args",
        "flash_app_args",
        "flash_bootloader_args",
        "flash_project_args",
        "partition_table/flash_args",
    ):
        source = esp_build / relative
        if source.is_symlink():
            raise ReleaseError(f"optional ESP32 flash argument must not be a symlink: {source}")
        if source.exists():
            if not source.is_file() or not within(esp_build, source.resolve()):
                raise ReleaseError(
                    f"optional ESP32 flash argument must be a regular file in the build directory: {source}"
                )
            result.append(
                {
                    "target": "esp32",
                    "role": "flash-arguments",
                    "source": source,
                    "relative": Path("esp32") / relative,
                }
            )
    return result


def esp32_flash_offsets(
    firmware_root: Path, profile: dict[str, Any], artifacts: list[dict[str, Any]]
) -> dict[str, str]:
    flasher_args = profile_path(
        firmware_root,
        nested(profile, ("targets", "esp32", "artifacts", "flasherArgs")),
        "targets.esp32.artifacts.flasherArgs",
    )
    data = load_json(flasher_args)
    flash_files = data.get("flash_files")
    if not isinstance(flash_files, dict):
        raise ReleaseError("ESP32 flasher_args.json requires a flash_files object")

    expected_sources = {
        item["source"].resolve(): item["role"]
        for item in artifacts
        if item["target"] == "esp32"
        and item["role"] in {"bootloader", "partition-table", "application"}
    }
    offsets: dict[str, str] = {}
    build_root = flasher_args.parent.resolve()
    for offset, relative_value in flash_files.items():
        if not isinstance(relative_value, str) or not relative_value or Path(relative_value).is_absolute():
            raise ReleaseError("ESP32 flash_files values must be non-empty relative paths")
        source = (build_root / relative_value).resolve()
        if not within(build_root, source):
            raise ReleaseError(f"ESP32 flash file escapes the build directory: {relative_value}")
        role = expected_sources.get(source)
        if role is not None:
            if role in offsets:
                raise ReleaseError(f"ESP32 flasher_args.json duplicates the {role} image")
            offsets[role] = canonical_offset(offset, f"ESP32 {role} offset")

    required_roles = {"bootloader", "partition-table", "application"}
    if set(offsets) != required_roles:
        raise ReleaseError(
            "ESP32 flasher_args.json does not map all required images; "
            f"missing={sorted(required_roles - set(offsets))}"
        )
    configured = nested(profile, ("targets", "esp32", "flashOffsets"))
    expected_offsets = {
        "bootloader": canonical_offset(configured["bootloader"], "ESP32 bootloader offset"),
        "partition-table": canonical_offset(
            configured["partitionTable"], "ESP32 partition table offset"
        ),
        "application": canonical_offset(configured["application"], "ESP32 application offset"),
    }
    if offsets != expected_offsets:
        raise ReleaseError(
            f"ESP32 flasher offsets {offsets} do not match release profile {expected_offsets}"
        )
    return offsets


def artifact_record(
    target: str, role: str, relative: Path, path: Path, offset: str | None = None
) -> dict[str, Any]:
    result = {
        "target": target,
        "role": role,
        "path": relative.as_posix(),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }
    if offset is not None:
        result["offset"] = offset
    return result


def cmake_cache_values(path: Path) -> dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except FileNotFoundError as exc:
        raise ReleaseError(f"missing build metadata: {path}") from exc
    result: dict[str, str] = {}
    for line in lines:
        if not line or line.startswith(("#", "//")) or "=" not in line or ":" not in line:
            continue
        key_and_type, value = line.split("=", 1)
        key, _ = key_and_type.split(":", 1)
        result[key] = value
    return result


def command_version(command: str) -> str:
    try:
        result = subprocess.run(
            [command, "--version"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        raise ReleaseError(f"unable to read {command} version after build") from exc
    first_line = result.stdout.splitlines()[0].strip() if result.stdout.splitlines() else ""
    if not first_line:
        raise ReleaseError(f"empty {command} version output")
    return first_line


def esp32_effective_build_policy(
    firmware_root: Path, profile: dict[str, Any]
) -> dict[str, Any]:
    policy = nested(profile, ("targets", "esp32", "buildPolicy"))
    if not isinstance(policy, dict) or not isinstance(policy.get("protocolV2"), bool):
        raise ReleaseError("ESP32 buildPolicy.protocolV2 must be boolean")
    for key in ("flashMode", "flashSize", "flashFrequency"):
        if not isinstance(policy.get(key), str) or not policy[key]:
            raise ReleaseError(f"ESP32 buildPolicy.{key} must be a non-empty string")

    sdkconfig = load_json(firmware_root / "esp32/build/config/sdkconfig.json")
    expected_sdkconfig = {
        "NOSTOS_PROTOCOL_V2": policy["protocolV2"],
        "ESPTOOLPY_FLASHMODE": policy["flashMode"],
        "ESPTOOLPY_FLASHSIZE": policy["flashSize"],
        "ESPTOOLPY_FLASHFREQ": policy["flashFrequency"],
    }
    for key, expected in expected_sdkconfig.items():
        if sdkconfig.get(key) != expected:
            raise ReleaseError(
                f"ESP32 effective config {key}={sdkconfig.get(key)!r}, expected {expected!r}"
            )

    flasher_args = load_json(firmware_root / "esp32/build/flasher_args.json")
    flash_settings = flasher_args.get("flash_settings")
    expected_flash_settings = {
        "flash_mode": policy["flashMode"],
        "flash_size": policy["flashSize"],
        "flash_freq": policy["flashFrequency"],
    }
    if not isinstance(flash_settings, dict):
        raise ReleaseError("ESP32 flasher_args.json requires flash_settings")
    for key, expected in expected_flash_settings.items():
        if flash_settings.get(key) != expected:
            raise ReleaseError(
                f"ESP32 flash setting {key}={flash_settings.get(key)!r}, expected {expected!r}"
            )
    return {
        "protocolV2": policy["protocolV2"],
        "flashMode": policy["flashMode"],
        "flashSize": policy["flashSize"],
        "flashFrequency": policy["flashFrequency"],
    }


def build_metadata(
    firmware_root: Path, profile: dict[str, Any], target: str, firmware_version: str
) -> dict[str, Any]:
    if target == "stm32":
        cache = cmake_cache_values(firmware_root / "stm32/build/Release/CMakeCache.txt")
        policy = nested(profile, ("targets", "stm32", "buildPolicy"))
        expected_options = {
            "NOSTOS_PROTOCOL_V2": "ON" if policy.get("protocolV2") else "OFF",
            "BUTTON_OUTPUT_TEST": "ON" if policy.get("buttonOutputTest") else "OFF",
            "SSD1306_DISPLAY": "ON" if policy.get("ssd1306Display") else "OFF",
            "MPU6050_SENSOR": "ON" if policy.get("mpu6050Sensor") else "OFF",
            "DHT11_SENSOR": "ON" if policy.get("dht11Sensor") else "OFF",
        }
        if cache.get("CMAKE_GENERATOR") != policy.get("generator"):
            raise ReleaseError(
                "STM32 build generator does not match the release profile: "
                f"{cache.get('CMAKE_GENERATOR')!r}"
            )
        for option, expected in expected_options.items():
            if cache.get(option) != expected:
                raise ReleaseError(
                    f"STM32 build option {option}={cache.get(option)!r}, expected {expected}"
                )
        return {
            "generator": cache["CMAKE_GENERATOR"],
            "options": expected_options,
            "compiler": command_version("arm-none-eabi-gcc"),
        }

    if target == "esp32":
        description = load_json(firmware_root / "esp32/build/project_description.json")
        expected_chip = nested(profile, ("targets", "esp32", "chip"))
        expected_idf = nested(profile, ("targets", "esp32", "espIdf"))
        if description.get("target") != expected_chip:
            raise ReleaseError(
                f"ESP32 build target is {description.get('target')!r}, expected {expected_chip!r}"
            )
        if description.get("git_revision") != expected_idf:
            raise ReleaseError(
                "ESP32 build ESP-IDF revision is "
                f"{description.get('git_revision')!r}, expected {expected_idf!r}"
            )
        if description.get("project_version") != firmware_version:
            raise ReleaseError(
                "ESP32 build project version is "
                f"{description.get('project_version')!r}, expected {firmware_version!r}"
            )
        return {
            "chip": expected_chip,
            "espIdf": expected_idf,
            "projectVersion": firmware_version,
            "buildPolicy": esp32_effective_build_policy(firmware_root, profile),
            "idfCommand": command_version("idf.py"),
        }

    raise ReleaseError(f"unknown build receipt target: {target}")


def build_receipt_path(firmware_root: Path, target: str) -> Path:
    return firmware_root / "out/build-receipts" / f"{target}.json"


def selected_build_artifacts(
    firmware_root: Path, profile: dict[str, Any], target: str
) -> list[dict[str, Any]]:
    selected = [item for item in required_artifacts(firmware_root, profile) if item["target"] == target]
    missing = [
        str(item["source"])
        for item in selected
        if not item["source"].is_file() or item["source"].is_symlink()
    ]
    if missing:
        raise ReleaseError(
            f"{target} build artifacts are missing; build receipt was not created:\n"
            + "\n".join(missing)
        )
    return selected


def record_build(args: argparse.Namespace) -> None:
    firmware_root = args.firmware_root.resolve()
    repo_root = args.repo_root.resolve()
    canonical_profile = canonical_profile_path(firmware_root, args.profile)
    profile = validate_profile(firmware_root, canonical_profile)
    version_file = profile_path(firmware_root, profile["versionFile"], "versionFile")
    try:
        firmware_version = version_file.read_text(encoding="utf-8").strip()
    except FileNotFoundError as exc:
        raise ReleaseError(f"missing firmware version file: {version_file}") from exc
    commit, clean = git_state(repo_root)
    selected = selected_build_artifacts(firmware_root, profile, args.target)
    metadata = build_metadata(firmware_root, profile, args.target, firmware_version)
    records = [
        artifact_record(
            item["target"],
            item["role"],
            item["source"].relative_to(firmware_root),
            item["source"],
        )
        for item in selected
    ]
    receipt = {
        "schemaVersion": 1,
        "target": args.target,
        "source": {"commit": commit, "clean": clean},
        "firmwareVersion": firmware_version,
        "profileSha256": sha256(canonical_profile),
        "buildMetadata": metadata,
        "artifacts": records,
        "createdAt": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat(),
    }
    destination = build_receipt_path(firmware_root, args.target)
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{args.target}.", suffix=".json", dir=destination.parent
    )
    os.close(descriptor)
    temporary = Path(temporary_name)
    try:
        temporary.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    print(
        f"build receipt: PASS\ntarget: {args.target}\nreceipt: {destination}\n"
        f"sourceClean: {str(clean).lower()}"
    )


def verify_build_receipt(
    firmware_root: Path,
    profile: dict[str, Any],
    profile_file: Path,
    target: str,
    commit: str,
    firmware_version: str,
) -> None:
    receipt_file = build_receipt_path(firmware_root, target)
    receipt = load_json(receipt_file)
    if receipt.get("schemaVersion") != 1 or receipt.get("target") != target:
        raise ReleaseError(f"invalid {target} build receipt schema or target")
    source = receipt.get("source")
    if not isinstance(source, dict) or source.get("commit") != commit or source.get("clean") is not True:
        raise ReleaseError(
            f"{target} build receipt must come from clean current HEAD {commit}"
        )
    if receipt.get("firmwareVersion") != firmware_version:
        raise ReleaseError(f"{target} build receipt firmware version does not match")
    if receipt.get("profileSha256") != sha256(profile_file):
        raise ReleaseError(f"{target} build receipt profile does not match")
    metadata = receipt.get("buildMetadata")
    if not isinstance(metadata, dict):
        raise ReleaseError(f"{target} build receipt metadata must be an object")
    if target == "stm32":
        cache = cmake_cache_values(firmware_root / "stm32/build/Release/CMakeCache.txt")
        policy = nested(profile, ("targets", "stm32", "buildPolicy"))
        expected_options = {
            "NOSTOS_PROTOCOL_V2": "ON" if policy.get("protocolV2") else "OFF",
            "BUTTON_OUTPUT_TEST": "ON" if policy.get("buttonOutputTest") else "OFF",
            "SSD1306_DISPLAY": "ON" if policy.get("ssd1306Display") else "OFF",
            "MPU6050_SENSOR": "ON" if policy.get("mpu6050Sensor") else "OFF",
            "DHT11_SENSOR": "ON" if policy.get("dht11Sensor") else "OFF",
        }
        if (
            metadata.get("generator") != cache.get("CMAKE_GENERATOR")
            or metadata.get("generator") != policy.get("generator")
            or metadata.get("options") != expected_options
            or not isinstance(metadata.get("compiler"), str)
            or not metadata["compiler"]
        ):
            raise ReleaseError("STM32 build receipt toolchain or options do not match")
    else:
        description = load_json(firmware_root / "esp32/build/project_description.json")
        if (
            metadata.get("chip") != nested(profile, ("targets", "esp32", "chip"))
            or metadata.get("espIdf") != nested(profile, ("targets", "esp32", "espIdf"))
            or metadata.get("projectVersion") != firmware_version
            or metadata.get("buildPolicy") != esp32_effective_build_policy(firmware_root, profile)
            or not isinstance(metadata.get("idfCommand"), str)
            or not metadata["idfCommand"]
            or description.get("target") != metadata.get("chip")
            or description.get("git_revision") != metadata.get("espIdf")
            or description.get("project_version") != firmware_version
        ):
            raise ReleaseError("ESP32 build receipt toolchain or project metadata do not match")

    receipt_artifacts = receipt.get("artifacts")
    if not isinstance(receipt_artifacts, list):
        raise ReleaseError(f"{target} build receipt artifacts must be an array")
    selected = selected_build_artifacts(firmware_root, profile, target)
    expected_records = [
        artifact_record(
            item["target"],
            item["role"],
            item["source"].relative_to(firmware_root),
            item["source"],
        )
        for item in selected
    ]
    if receipt_artifacts != expected_records:
        raise ReleaseError(f"{target} build artifacts differ from the build receipt")


def make_read_only(root: Path) -> None:
    for path in sorted(root.rglob("*"), reverse=True):
        if path.is_file():
            path.chmod(stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
        elif path.is_dir():
            path.chmod(
                stat.S_IRUSR
                | stat.S_IXUSR
                | stat.S_IRGRP
                | stat.S_IXGRP
                | stat.S_IROTH
                | stat.S_IXOTH
            )
    root.chmod(
        stat.S_IRUSR
        | stat.S_IXUSR
        | stat.S_IRGRP
        | stat.S_IXGRP
        | stat.S_IROTH
        | stat.S_IXOTH
    )


def package(args: argparse.Namespace) -> None:
    firmware_root = args.firmware_root.resolve()
    repo_root = args.repo_root.resolve()
    canonical_profile = canonical_profile_path(firmware_root, args.profile)
    profile = validate_profile(firmware_root, canonical_profile)
    version = semver(args.version)
    if profile["status"] != "approved":
        raise ReleaseError("official packages require an approved release profile; current status is draft")
    version_file = profile_path(firmware_root, profile["versionFile"], "versionFile")
    try:
        declared_version = version_file.read_text(encoding="utf-8").strip()
    except FileNotFoundError as exc:
        raise ReleaseError(f"missing firmware version file: {version_file}") from exc
    if declared_version != f"v{version}":
        raise ReleaseError(
            f"firmware version mismatch: {version_file} contains {declared_version!r}, expected 'v{version}'"
        )

    commit = require_clean_git(repo_root)
    release_id = f"nostos-v{version}"
    require_annotated_release_tag(repo_root, release_id, commit)
    for target in ("stm32", "esp32"):
        verify_build_receipt(
            firmware_root,
            profile,
            canonical_profile,
            target,
            commit,
            declared_version,
        )
    artifacts = required_artifacts(firmware_root, profile)
    missing = [
        str(item["source"])
        for item in artifacts[:5]
        if not item["source"].is_file() or item["source"].is_symlink()
    ]
    if missing:
        raise ReleaseError(
            "required build artifacts are missing; package never runs a build:\n" + "\n".join(missing)
        )

    output_root = profile_path(
        firmware_root, nested(profile, ("package", "outputDirectory")), "package.outputDirectory"
    )
    destination = output_root / release_id
    if destination.exists():
        raise ReleaseError(f"release output already exists and will not be overwritten: {destination}")

    output_root.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{release_id}.", dir=output_root))
    try:
        esp_offsets = esp32_flash_offsets(firmware_root, profile, artifacts)
        stm32_offset = canonical_offset(
            nested(profile, ("targets", "stm32", "flashAddress")), "STM32 flashAddress"
        )
        records: list[dict[str, Any]] = []
        for item in artifacts:
            target = item["target"]
            role = item["role"]
            source = item["source"]
            relative = item["relative"]
            destination_file = staging / relative
            destination_file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination_file)
            offset = None
            if target == "stm32" and role == "application":
                offset = stm32_offset
            elif target == "esp32" and role in esp_offsets:
                offset = esp_offsets[role]
            records.append(artifact_record(target, role, relative, destination_file, offset))

        profile_relative = Path("profile/release.json")
        packaged_profile = staging / profile_relative
        packaged_profile.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.profile.resolve(), packaged_profile)
        records.append(
            artifact_record(
                "bundle", "release-profile", profile_relative, packaged_profile
            )
        )

        record_by_target_role = {(item["target"], item["role"]): item for item in records}
        stm32_app = record_by_target_role[("stm32", "application")]
        esp32_app = record_by_target_role[("esp32", "application")]
        esp32_partition = record_by_target_role[("esp32", "partition-table")]

        manifest = {
            "schemaVersion": 1,
            "release": {
                "id": release_id,
                "version": version,
                "firmwareVersion": declared_version,
                "gitCommit": commit,
                "gitTag": release_id,
                "createdAt": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat(),
                "profile": profile["profile"],
                "profileStatus": profile["status"],
                "profileSha256": sha256(args.profile.resolve()),
            },
            "protocols": profile["protocols"],
            "artifacts": records,
            "flashPlan": {
                "stm32": {
                    "chip": nested(profile, ("targets", "stm32", "chip")),
                    "mode": "application-only",
                    "entries": [
                        {
                            "role": stm32_app["role"],
                            "path": stm32_app["path"],
                            "offset": stm32_app["offset"],
                        }
                    ],
                },
                "esp32": {
                    "chip": nested(profile, ("targets", "esp32", "chip")),
                    "mode": nested(profile, ("targets", "esp32", "defaultFlashMode")),
                    "partitionLayout": nested(profile, ("targets", "esp32", "partitionLayout")),
                    "partitionTableSha256": esp32_partition["sha256"],
                    "entries": [
                        {
                            "role": esp32_app["role"],
                            "path": esp32_app["path"],
                            "offset": esp32_app["offset"],
                        }
                    ],
                },
            },
        }
        (staging / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(staging, destination)
        make_read_only(destination)
    except BaseException:
        if staging.exists():
            shutil.rmtree(staging)
        raise
    print(f"package: PASS\nrelease: {destination}\nartifacts: {len(records)}")


def resolve_release(firmware_root: Path, profile_file: Path, value: str) -> Path:
    raw = Path(value).expanduser()
    if raw.exists() or raw.is_absolute() or "/" in value:
        return raw.resolve()
    profile = validate_profile(firmware_root, profile_file.resolve())
    output_root = profile_path(
        firmware_root, nested(profile, ("package", "outputDirectory")), "package.outputDirectory"
    )
    release_id = value
    if SEMVER_RE.fullmatch(value):
        release_id = f"nostos-v{value}"
    return (output_root / release_id).resolve()


def verify_release(release_dir: Path) -> dict[str, Any]:
    if not release_dir.is_dir():
        raise ReleaseError(f"release directory not found: {release_dir}")
    manifest_path = release_dir / "manifest.json"
    if not manifest_path.is_file() or manifest_path.is_symlink():
        raise ReleaseError("release manifest must be one regular, non-symlink root manifest.json")
    manifest = load_json(manifest_path)
    if manifest.get("schemaVersion") != 1:
        raise ReleaseError("unsupported manifest schemaVersion (expected 1)")
    release = manifest.get("release")
    artifacts = manifest.get("artifacts")
    protocols = manifest.get("protocols")
    flash_plan = manifest.get("flashPlan")
    if (
        not isinstance(release, dict)
        or not isinstance(artifacts, list)
        or not isinstance(protocols, dict)
        or not isinstance(flash_plan, dict)
    ):
        raise ReleaseError(
            "manifest requires release, protocols, flashPlan objects and an artifacts array"
        )
    release_id = release.get("id")
    version = release.get("version")
    if not isinstance(release_id, str) or not isinstance(version, str):
        raise ReleaseError("manifest release.id and release.version must be strings")
    semver(version)
    if release_id != f"nostos-v{version}" or release_dir.name != release_id:
        raise ReleaseError("release ID, version, and directory name do not agree")
    if release.get("firmwareVersion") != f"v{version}":
        raise ReleaseError("release firmwareVersion does not match release.version")
    if release.get("profileStatus") != "approved":
        raise ReleaseError("release profileStatus must be approved")
    if release.get("gitTag") != release_id:
        raise ReleaseError("release gitTag does not match release.id")
    if not isinstance(release.get("gitCommit"), str) or not re.fullmatch(
        r"[0-9a-f]{40}", release["gitCommit"]
    ):
        raise ReleaseError("release gitCommit must be a full lowercase Git object ID")
    if not isinstance(release.get("profileSha256"), str) or not re.fullmatch(
        r"[0-9a-f]{64}", release["profileSha256"]
    ):
        raise ReleaseError("release profileSha256 must be a lowercase SHA-256")
    if not {"uart", "mesh"}.issubset(protocols):
        raise ReleaseError("manifest protocols must contain uart and mesh contracts")
    for protocol_name in ("uart", "mesh"):
        contract = protocols.get(protocol_name)
        if not isinstance(contract, dict) or not isinstance(contract.get("version"), int):
            raise ReleaseError(f"manifest protocol {protocol_name} requires an integer version")
        if contract["version"] < 1:
            raise ReleaseError(f"manifest protocol {protocol_name} version must be positive")

    expected: set[str] = set()
    targets: set[str] = set()
    for index, artifact in enumerate(artifacts):
        if not isinstance(artifact, dict):
            raise ReleaseError(f"artifact {index} must be an object")
        path_value = artifact.get("path")
        target = artifact.get("target")
        role = artifact.get("role")
        expected_bytes = artifact.get("bytes")
        expected_sha = artifact.get("sha256")
        if not isinstance(path_value, str) or not path_value:
            raise ReleaseError(f"artifact {index} has an invalid path")
        if target not in {"bundle", "stm32", "esp32"} or not isinstance(role, str):
            raise ReleaseError(f"artifact {index} has an invalid target or role")
        relative = Path(path_value)
        if relative.is_absolute() or ".." in relative.parts or path_value in expected:
            raise ReleaseError(f"artifact {index} path is unsafe or duplicated: {path_value}")
        path = (release_dir / relative).resolve()
        if not within(release_dir.resolve(), path) or not path.is_file() or path.is_symlink():
            raise ReleaseError(f"artifact is missing, unsafe, or not a regular file: {path_value}")
        if not isinstance(expected_bytes, int) or expected_bytes < 0:
            raise ReleaseError(f"artifact {path_value} has an invalid byte count")
        if not isinstance(expected_sha, str) or not re.fullmatch(r"[0-9a-f]{64}", expected_sha):
            raise ReleaseError(f"artifact {path_value} has an invalid SHA-256")
        if path.stat().st_size != expected_bytes:
            raise ReleaseError(f"size mismatch: {path_value}")
        if sha256(path) != expected_sha:
            raise ReleaseError(f"SHA-256 mismatch: {path_value}")
        offset = artifact.get("offset")
        if offset is not None:
            canonical_offset(offset, f"artifact {path_value} offset")
        expected.add(path_value)
        targets.add(target)

    if targets != {"bundle", "stm32", "esp32"}:
        raise ReleaseError("manifest must contain bundle, stm32, and esp32 artifacts")
    role_pairs = {(item["target"], item["role"]) for item in artifacts}
    required_roles = {
        ("stm32", "application"),
        ("esp32", "bootloader"),
        ("esp32", "partition-table"),
        ("esp32", "application"),
        ("esp32", "flasher-arguments"),
        ("bundle", "release-profile"),
    }
    if not required_roles.issubset(role_pairs):
        raise ReleaseError(
            f"manifest is missing required target roles: {sorted(required_roles - role_pairs)}"
        )
    profile_artifact = next(
        item
        for item in artifacts
        if item["target"] == "bundle" and item["role"] == "release-profile"
    )
    if release["profileSha256"] != profile_artifact["sha256"]:
        raise ReleaseError("release profileSha256 does not match the packaged profile")
    packaged_profile = load_json(release_dir / profile_artifact["path"])
    if packaged_profile.get("profile") != "release" or packaged_profile.get("status") != "approved":
        raise ReleaseError("packaged release profile must be approved")

    artifact_by_target_path = {(item["target"], item["path"]): item for item in artifacts}
    for target, expected_chip in (("stm32", "STM32F411xC_xE"), ("esp32", "esp32s3")):
        target_plan = flash_plan.get(target)
        if not isinstance(target_plan, dict):
            raise ReleaseError(f"flashPlan.{target} must be an object")
        if target_plan.get("chip") != expected_chip or target_plan.get("mode") != "application-only":
            raise ReleaseError(f"flashPlan.{target} has an invalid chip or mode")
        if target == "esp32":
            partition_layout = target_plan.get("partitionLayout")
            partition_sha = target_plan.get("partitionTableSha256")
            partition_artifact = artifact_by_target_path.get(
                ("esp32", "esp32/partition_table/partition-table.bin")
            )
            if not isinstance(partition_layout, str) or not partition_layout:
                raise ReleaseError("flashPlan.esp32 requires a partitionLayout")
            if (
                not isinstance(partition_sha, str)
                or not re.fullmatch(r"[0-9a-f]{64}", partition_sha)
                or partition_artifact is None
                or partition_artifact.get("sha256") != partition_sha
            ):
                raise ReleaseError(
                    "flashPlan.esp32 partitionTableSha256 must match the packaged partition table"
                )
        entries = target_plan.get("entries")
        if not isinstance(entries, list) or len(entries) != 1:
            raise ReleaseError(f"flashPlan.{target} must contain one application entry")
        entry = entries[0]
        if not isinstance(entry, dict) or entry.get("role") != "application":
            raise ReleaseError(f"flashPlan.{target} entry must be the application")
        entry_path = entry.get("path")
        entry_offset = entry.get("offset")
        if not isinstance(entry_path, str) or not isinstance(entry_offset, str):
            raise ReleaseError(f"flashPlan.{target} entry requires path and offset strings")
        canonical_offset(entry_offset, f"flashPlan.{target} offset")
        artifact = artifact_by_target_path.get((target, entry_path))
        if artifact is None or artifact.get("role") != "application":
            raise ReleaseError(f"flashPlan.{target} does not reference its application artifact")
        if entry.get("offset") != artifact.get("offset"):
            raise ReleaseError(f"flashPlan.{target} offset does not match the artifact")
    actual: set[str] = set()
    for path in release_dir.rglob("*"):
        relative = path.relative_to(release_dir).as_posix()
        if path.is_symlink():
            raise ReleaseError(f"release contains a symlink: {relative}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ReleaseError(f"release contains a non-regular entry: {relative}")
        if relative != "manifest.json":
            actual.add(relative)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise ReleaseError(f"release file set mismatch; missing={missing}, extra={extra}")
    return manifest


def verify(args: argparse.Namespace) -> None:
    firmware_root = args.firmware_root.resolve()
    release_dir = resolve_release(firmware_root, args.profile, args.release)
    manifest = verify_release(release_dir)
    print(
        "verify: PASS\n"
        f"release: {release_dir}\n"
        f"artifacts: {len(manifest['artifacts'])}\n"
        "signature: NOT_PRESENT (self-consistency only; authenticity not verified)"
    )


def inventory_device(inventory_file: Path, node: str, target: str) -> dict[str, Any]:
    inventory = load_json(inventory_file)
    if inventory.get("schemaVersion") != 1 or not isinstance(inventory.get("devices"), list):
        raise ReleaseError("local inventory requires schemaVersion 1 and a devices array")
    matches = [
        item
        for item in inventory["devices"]
        if isinstance(item, dict) and item.get("id") == node and item.get("target") == target
    ]
    if len(matches) != 1:
        raise ReleaseError(f"local inventory must contain exactly one {target} device for node {node!r}")
    device = matches[0]
    if device.get("enabled") is not True:
        raise ReleaseError(f"inventory device {node!r}/{target} is not enabled")
    if target == "stm32":
        if device.get("transport") != "stlink":
            raise ReleaseError("STM32 inventory transport must be stlink")
        selector = device.get("serial")
        selector_name = "serial"
    else:
        if device.get("transport") != "serial":
            raise ReleaseError("ESP32 inventory transport must be serial")
        selector = device.get("port")
        selector_name = "port"
    if (
        not isinstance(selector, str)
        or not selector
        or selector.startswith("REPLACE_WITH_")
    ):
        raise ReleaseError(f"inventory device {node!r}/{target} has no usable {selector_name}")
    return device


def require_dev_artifact(path: Path, label: str) -> Path:
    if not path.is_file() or path.is_symlink():
        raise ReleaseError(f"{label} is missing or is not a regular file: {path}")
    if path.stat().st_size <= 0:
        raise ReleaseError(f"{label} is empty: {path}")
    return path


def run_checked(
    command: list[str], label: str, *, environment: dict[str, str] | None = None
) -> None:
    try:
        subprocess.run(command, check=True, env=environment)
    except FileNotFoundError as exc:
        raise ReleaseError(f"{label} executable not found: {command[0]}") from exc
    except subprocess.CalledProcessError as exc:
        raise ReleaseError(f"{label} failed with exit code {exc.returncode}") from exc


def stm32_dev_hardware_options(
    device: dict[str, Any], target_profile: dict[str, Any]
) -> dict[str, str]:
    policy = nested(target_profile, ("buildPolicy",))
    hardware = device.get("hardwareProfile")
    if hardware is None:
        hardware = {}
    if not isinstance(hardware, dict):
        raise ReleaseError("STM32 inventory hardwareProfile must be an object")

    fields = {
        "NOSTOS_STM32_SSD1306_DISPLAY": "ssd1306Display",
        "NOSTOS_STM32_MPU6050_SENSOR": "mpu6050Sensor",
        "NOSTOS_STM32_DHT11_SENSOR": "dht11Sensor",
    }
    options: dict[str, str] = {}
    for environment_name, profile_name in fields.items():
        value = hardware.get(profile_name, policy.get(profile_name))
        if not isinstance(value, bool):
            raise ReleaseError(
                f"STM32 inventory hardwareProfile.{profile_name} must be boolean"
            )
        options[environment_name] = "ON" if value else "OFF"
    return options


def validate_esp_dev_partition(
    firmware_root: Path, target_profile: dict[str, Any], device: dict[str, Any]
) -> Path:
    partition = require_dev_artifact(
        profile_path(
            firmware_root,
            nested(target_profile, ("artifacts", "partitionTable")),
            "targets.esp32.artifacts.partitionTable",
        ),
        "ESP32 partition table",
    )
    if device.get("partitionLayout") != target_profile["partitionLayout"]:
        raise ReleaseError("ESP32 inventory partitionLayout does not match the current build profile")
    if device.get("partitionTableSha256") != sha256(partition):
        raise ReleaseError("ESP32 inventory partitionTableSha256 does not match the current build")
    return partition


def dev_flash(args: argparse.Namespace) -> None:
    firmware_root = args.firmware_root.resolve()
    canonical_profile = canonical_profile_path(firmware_root, args.profile)
    profile = validate_profile(firmware_root, canonical_profile)
    device = inventory_device(args.inventory.resolve(), args.node, args.target)
    execute = bool(args.execute)

    if args.target == "stm32":
        target_profile = nested(profile, ("targets", "stm32"))
        hardware_options = stm32_dev_hardware_options(device, target_profile)
        artifact = profile_path(
            firmware_root, target_profile["artifact"], "targets.stm32.artifact"
        )
        offset = canonical_offset(target_profile["flashAddress"], "STM32 flashAddress")
        tool_name = "st-flash"
        preserved = "option bytes and OTP; no chip erase"
    else:
        target_profile = nested(profile, ("targets", "esp32"))
        artifact = profile_path(
            firmware_root,
            nested(target_profile, ("artifacts", "application")),
            "targets.esp32.artifacts.application",
        )
        offset = canonical_offset(
            nested(target_profile, ("flashOffsets", "application")),
            "ESP32 application offset",
        )
        tool_name = "esptool.py"
        preserved = "bootloader, partition table, NVS, and provisioning"

    print("development flash plan")
    print(f"target: {args.target}")
    print(f"node: {args.node}")
    print(f"artifact: {artifact}")
    print(f"offset: {offset}")
    print(f"preserved: {preserved}")
    print("scope: one application image on one named node")
    print("release receipt/package verification: SKIPPED (development path)")
    print("external full read-back: SKIPPED (development path)")
    if args.target == "stm32":
        print(
            "hardware profile: "
            f"SSD1306={hardware_options['NOSTOS_STM32_SSD1306_DISPLAY']} "
            f"MPU6050={hardware_options['NOSTOS_STM32_MPU6050_SENSOR']} "
            f"DHT11={hardware_options['NOSTOS_STM32_DHT11_SENSOR']}"
        )

    if not execute:
        artifact_state = "ready" if artifact.is_file() and not artifact.is_symlink() else "not built"
        print(f"artifact state: {artifact_state}")
        print("build action: incremental target build on execute")
        print("hardware action: NONE")
        return

    if args.target == "esp32":
        partition_path = profile_path(
            firmware_root,
            nested(target_profile, ("artifacts", "partitionTable")),
            "targets.esp32.artifacts.partitionTable",
        )
        if partition_path.exists():
            validate_esp_dev_partition(firmware_root, target_profile, device)

    tool = shutil.which(tool_name)
    if tool is None:
        if args.target == "esp32":
            raise ReleaseError(
                "esptool.py not found; activate the ESP-IDF v5.5.5 environment before dev Flash"
            )
        raise ReleaseError("st-flash not found; install stlink before dev Flash")

    build_script = require_dev_artifact(firmware_root / "build.sh", "firmware build script")
    build_environment = None
    if args.target == "stm32":
        build_environment = os.environ.copy()
        build_environment.update(hardware_options)
    run_checked(
        ["bash", str(build_script), args.target],
        f"{args.target} incremental build",
        environment=build_environment,
    )
    artifact = require_dev_artifact(artifact, f"{args.target} application image")

    if args.target == "stm32":
        command = [
            tool,
            "--serial",
            str(device["serial"]),
            "--reset",
            "write",
            str(artifact),
            offset,
        ]
    else:
        validate_esp_dev_partition(firmware_root, target_profile, device)
        policy = nested(target_profile, ("buildPolicy",))
        command = [
            tool,
            "--chip",
            str(target_profile["chip"]),
            "--port",
            str(device["port"]),
            "--before",
            "default_reset",
            "--after",
            "hard_reset",
            "write_flash",
            "--flash_mode",
            str(policy["flashMode"]),
            "--flash_freq",
            str(policy["flashFrequency"]),
            "--flash_size",
            str(policy["flashSize"]),
            offset,
            str(artifact),
        ]

    print(f"application bytes: {artifact.stat().st_size}")
    print(f"application sha256: {sha256(artifact)}")
    run_checked(command, f"{args.target} development Flash")
    print("development Flash: PASS")
    print("hardware functional verification: NOT PERFORMED")


def plan_flash(args: argparse.Namespace) -> None:
    firmware_root = args.firmware_root.resolve()
    release_dir = resolve_release(firmware_root, args.profile, args.release)
    manifest = verify_release(release_dir)
    device = inventory_device(args.inventory.resolve(), args.node, args.target)
    target_plan = manifest["flashPlan"][args.target]
    if args.target == "esp32":
        if device.get("partitionLayout") != target_plan["partitionLayout"]:
            raise ReleaseError(
                "ESP32 inventory partitionLayout does not match the release package"
            )
        if device.get("partitionTableSha256") != target_plan["partitionTableSha256"]:
            raise ReleaseError(
                "ESP32 inventory partitionTableSha256 does not match the packaged partition table"
            )
    artifact_by_path = {item["path"]: item for item in manifest["artifacts"]}
    print("flash plan: VERIFIED DRY RUN")
    print(f"release: {release_dir}")
    print(f"target: {args.target}")
    print(f"chip: {target_plan['chip']}")
    print(f"mode: {target_plan['mode']}")
    print(f"node: {args.node}")
    if args.target == "stm32":
        print(f"transport: stlink serial={device['serial']}")
        print("reset impact: reset is required after a future write")
        print("preserved: option bytes and OTP are outside this plan")
    else:
        print(f"transport: serial port={device['port']}")
        print("reset impact: serial connection may reset the ESP32 before and after a future write")
        print("preserved: NVS and partition table (application-only plan)")
    for entry in target_plan["entries"]:
        item = artifact_by_path[entry["path"]]
        print(
            f"write: {entry['offset']} {item['role']} {item['path']} "
            f"({item['bytes']} bytes, sha256={item['sha256']})"
        )
    print("hardware action: NONE")
    print("build action: NONE")
    print("actual flashing is intentionally unavailable in this command")


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    sub = result.add_subparsers(dest="command", required=True)

    validate_parser = sub.add_parser("validate-profile")
    validate_parser.add_argument("--firmware-root", type=Path, required=True)
    validate_parser.add_argument("--profile", type=Path, required=True)

    receipt_parser = sub.add_parser("record-build")
    receipt_parser.add_argument("--firmware-root", type=Path, required=True)
    receipt_parser.add_argument("--repo-root", type=Path, required=True)
    receipt_parser.add_argument("--profile", type=Path, required=True)
    receipt_parser.add_argument("--target", choices=("stm32", "esp32"), required=True)

    package_parser = sub.add_parser("package")
    package_parser.add_argument("--firmware-root", type=Path, required=True)
    package_parser.add_argument("--repo-root", type=Path, required=True)
    package_parser.add_argument("--profile", type=Path, required=True)
    package_parser.add_argument("--version", required=True)

    verify_parser = sub.add_parser("verify")
    verify_parser.add_argument("--firmware-root", type=Path, required=True)
    verify_parser.add_argument("--profile", type=Path, required=True)
    verify_parser.add_argument("--release", required=True)

    plan_parser = sub.add_parser("plan-flash")
    plan_parser.add_argument("--firmware-root", type=Path, required=True)
    plan_parser.add_argument("--profile", type=Path, required=True)
    plan_parser.add_argument("--inventory", type=Path, required=True)
    plan_parser.add_argument("--release", required=True)
    plan_parser.add_argument("--target", choices=("stm32", "esp32"), required=True)
    plan_parser.add_argument("--node", required=True)

    dev_parser = sub.add_parser("dev-flash")
    dev_parser.add_argument("--firmware-root", type=Path, required=True)
    dev_parser.add_argument("--profile", type=Path, required=True)
    dev_parser.add_argument("--inventory", type=Path, required=True)
    dev_parser.add_argument("--target", choices=("stm32", "esp32"), required=True)
    dev_parser.add_argument("--node", required=True)
    dev_action = dev_parser.add_mutually_exclusive_group(required=True)
    dev_action.add_argument("--dry-run", action="store_true")
    dev_action.add_argument("--execute", action="store_true")
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        if args.command == "validate-profile":
            profile = validate_profile(args.firmware_root, args.profile)
            print(
                f"OK   release profile        {args.profile} "
                f"(schema={profile['schemaVersion']}, status={profile['status']})"
            )
        elif args.command == "record-build":
            record_build(args)
        elif args.command == "package":
            package(args)
        elif args.command == "verify":
            verify(args)
        elif args.command == "plan-flash":
            plan_flash(args)
        elif args.command == "dev-flash":
            dev_flash(args)
        else:
            raise ReleaseError(f"unknown command: {args.command}")
    except ReleaseError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
