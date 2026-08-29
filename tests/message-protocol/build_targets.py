#!/usr/bin/env python3
"""Compile v2 with real installed toolchains; never install, flash or open ports."""
import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def environment():
    env = dict(os.environ)
    base = Path(env.get("NOSTOS_TOOLCHAINS", Path.home() / ".local/share/nostos-toolchains"))
    extra = []
    for tool, directory in (("arm-none-eabi-gcc", base / "arm-15.3-extracted/Payload/bin"),
                            ("ninja", base / "build-tools/bin")):
        if not shutil.which(tool) and directory.is_dir():
            extra.append(str(directory))
    env["PATH"] = os.pathsep.join([*extra, env.get("PATH", "")])
    if not env.get("ESP_IDF_PATH"):
        for path in (Path(env.get("IDF_PATH", "/nonexistent")), base / "esp-idf-v5.5.5",
                     Path.home() / "esp/esp-idf-v5.5.5"):
            if (path / "export.sh").is_file():
                env["ESP_IDF_PATH"] = str(path)
                break
    if "IDF_TOOLS_PATH" not in env and (base / "espressif-tools").is_dir():
        env["IDF_TOOLS_PATH"] = str(base / "espressif-tools")
    return env


def execute(command, cwd, env, log):
    print(f"BUILD {log.stem}: {log}", flush=True)
    with log.open("w") as stream:
        stream.write(json.dumps(command) + "\n")
        stream.flush()
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text()[-12000:], file=sys.stderr)
        raise RuntimeError(f"{log.stem} failed (exit {result.returncode})")


def main():
    env = environment()
    for tool in ("cmake", "ninja", "arm-none-eabi-gcc", "arm-none-eabi-size", "arm-none-eabi-nm"):
        if not shutil.which(tool, path=env["PATH"]):
            raise RuntimeError(f"Missing installed tool: {tool}; set PATH or NOSTOS_TOOLCHAINS")
    if not (Path(env.get("ESP_IDF_PATH", "/nonexistent")) / "export.sh").is_file():
        raise RuntimeError("Set ESP_IDF_PATH to an existing ESP-IDF v5.5.5 installation")
    # Only the existing local SDK example dependency is allowed here.
    manifest = ROOT / "firmware/esp32/main/idf_component.yml"
    actual = [line.strip() for line in manifest.read_text().splitlines()
              if line.strip() and not line.lstrip().startswith("#")]
    if actual != ["dependencies:", "example_init:",
                  "path: ${IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/example_init"]:
        raise RuntimeError("ESP dependencies changed: verify compatibility before target build")
    out = Path(tempfile.mkdtemp(prefix="nostos-protocol-targets-"))
    print(f"Target artifacts: {out}", flush=True)
    source = out / "source"
    ignore = shutil.ignore_patterns("build*", ".git", "managed_components", "__pycache__")
    for path in ("firmware/stm32", "firmware/esp32", "libs/protocol"):
        shutil.copytree(ROOT / path, source / path, ignore=ignore)
    hashes = {str(path.relative_to(source)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in source.rglob("*") if path.is_file()}
    (out / "source-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n")
    esp = source / "firmware/esp32"
    config = esp / "sdkconfig"
    lines = [line for line in config.read_text().splitlines()
             if not line.startswith(("CONFIG_NOSTOS_", "# CONFIG_NOSTOS_"))]
    # Build only: addresses intentionally stay zero, so this artifact cannot
    # accidentally start on guessed real board identities.
    config.write_text("\n".join(lines) + "\nCONFIG_NOSTOS_PROTOCOL_V2=y\n")
    evidence = {"protocol": 2, "status": "FAIL", "targets": {},
                "flash": "NOT_PERFORMED", "real_ble_rf": "NOT_TESTED"}
    try:
        for variant in ("Debug", "Release"):
            build = out / f"stm32-{variant}"
            execute(["cmake", "--preset", variant, "-B", str(build), "-DNOSTOS_PROTOCOL_V2=ON",
                     "-DCMAKE_C_FLAGS=-Werror"], source / "firmware/stm32", env,
                    out / f"stm32-{variant}-configure.log")
            execute(["cmake", "--build", str(build), "--parallel", "8"], ROOT, env,
                    out / f"stm32-{variant}-build.log")
            elf = build / "nostos_stm32.elf"
            if not elf.is_file():
                raise RuntimeError(f"Missing linked artifact: {elf}")
            cache = (build / "CMakeCache.txt").read_text()
            if "NOSTOS_PROTOCOL_V2:BOOL=ON" not in cache:
                raise RuntimeError("STM32 was not compiled with v2 enabled")
            execute(["arm-none-eabi-size", str(elf)], ROOT, env, out / f"stm32-{variant}-size.log")
            symbols_log = out / f"stm32-{variant}-symbols.log"
            execute(["arm-none-eabi-nm", str(elf)], ROOT, env, symbols_log)
            symbols = {line.split()[-1] for line in symbols_log.read_text().splitlines() if line.split()}
            required = {"message_protocol_service_init", "nostos_message_encode", "nostos_message_decode",
                        "audio_service_play", "vs1003b_play_process"}
            if not required <= symbols:
                raise RuntimeError(f"v2 path was removed from linked image: {sorted(required - symbols)}")
            evidence["targets"][f"stm32-{variant}"] = {"status": "PASS", "elf": str(elf)}
        # Existing wrapper checks idf.py --version and does a complete IDF build.
        execute(["bash", str(ROOT / "tools/testing/build-target.sh"), "esp32", str(esp),
                 str(out / "esp32-build")], ROOT, env, out / "esp32-build.log")
        elf = out / "esp32-build/nostos_esp32.elf"
        if not elf.is_file():
            raise RuntimeError(f"Missing linked artifact: {elf}")
        generated = (out / "esp32-build/config/sdkconfig.h").read_text()
        if "#define CONFIG_NOSTOS_PROTOCOL_V2 1" not in generated:
            raise RuntimeError("ESP32 was not compiled with v2 enabled")
        units = {Path(unit["file"]).name for unit in json.loads((out / "esp32-build/compile_commands.json").read_text())}
        if "bridge_runtime_v2.c" not in units or "bridge_runtime.c" in units:
            raise RuntimeError("Wrong ESP32 runtime was compiled")
        evidence["targets"]["esp32s3"] = {"status": "PASS", "elf": str(elf)}
        evidence["status"] = "PASS"
    finally:
        (out / "results.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("V2_TARGET_BUILDS=PASS; FLASH=NOT_PERFORMED; REAL_BLE_RF=NOT_TESTED")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as exc:
        print(f"V2_TARGET_BUILDS=FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
