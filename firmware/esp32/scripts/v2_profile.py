#!/usr/bin/env python3
"""Build/flash one verified NOSTOS v2 board profile without touching Mesh NVS."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
PROFILE_FILE = PROJECT / "profiles/v2.json"
BOARD_ORDER = ("76", "D6", "B6")


def load_profiles(path: Path = PROFILE_FILE) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("protocol") != 2 or data.get("target") != "esp32s3":
        raise ValueError("PROFILE_HEADER_INVALID")
    peers = data.get("peers")
    if not isinstance(peers, dict) or tuple(peers) != BOARD_ORDER:
        raise ValueError("PROFILE_BOARD_SET_INVALID")
    sources, addresses, serials = set(), set(), set()
    for board in BOARD_ORDER:
        peer = peers[board]
        source = peer.get("source")
        address_text = peer.get("mesh_address")
        serial = peer.get("usb_serial")
        if source not in (1, 2, 3) or not isinstance(address_text, str):
            raise ValueError("PROFILE_VALUE_INVALID")
        address = int(address_text, 16)
        if not 1 <= address <= 0x7FFF:
            raise ValueError("PROFILE_VALUE_INVALID")
        if not isinstance(serial, str) or not re.fullmatch(r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}", serial):
            raise ValueError("PROFILE_VALUE_INVALID")
        sources.add(source)
        addresses.add(address)
        serials.add(serial)
    if len(sources) != 3 or len(addresses) != 3 or len(serials) != 3:
        raise ValueError("PROFILE_IDENTITY_DUPLICATE")
    return data


def render_sdkconfig(board: str, output: Path, profiles: dict | None = None) -> dict[str, int]:
    profiles = profiles or load_profiles()
    peers = profiles["peers"]
    if board not in peers:
        raise ValueError("UNKNOWN_BOARD")
    base = PROJECT / "sdkconfig"
    lines = [line for line in base.read_text(encoding="utf-8").splitlines()
             if not re.match(r"(?:# )?CONFIG_NOSTOS_", line)]
    settings = {
        "CONFIG_NOSTOS_PROTOCOL_V2": 1,
        "CONFIG_NOSTOS_LOCAL_SOURCE": peers[board]["source"],
        "CONFIG_NOSTOS_SOURCE1_ADDRESS": int(peers["76"]["mesh_address"], 16),
        "CONFIG_NOSTOS_SOURCE2_ADDRESS": int(peers["D6"]["mesh_address"], 16),
        "CONFIG_NOSTOS_SOURCE3_ADDRESS": int(peers["B6"]["mesh_address"], 16),
    }
    lines.extend([
        "CONFIG_NOSTOS_PROTOCOL_V2=y",
        f"CONFIG_NOSTOS_LOCAL_SOURCE={settings['CONFIG_NOSTOS_LOCAL_SOURCE']}",
        f"CONFIG_NOSTOS_SOURCE1_ADDRESS=0x{settings['CONFIG_NOSTOS_SOURCE1_ADDRESS']:x}",
        f"CONFIG_NOSTOS_SOURCE2_ADDRESS=0x{settings['CONFIG_NOSTOS_SOURCE2_ADDRESS']:x}",
        f"CONFIG_NOSTOS_SOURCE3_ADDRESS=0x{settings['CONFIG_NOSTOS_SOURCE3_ADDRESS']:x}",
    ])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return settings


def require_free_verified_port(board: str, path: str, profiles: dict) -> None:
    if not re.fullmatch(r"/dev/(?:cu|tty)\.usbmodem[A-Za-z0-9_-]+", path):
        raise RuntimeError("UNEXPECTED_USB_PATH")
    try:
        from serial.tools import list_ports
    except ImportError as exc:
        raise RuntimeError("PYSERIAL_NOT_AVAILABLE") from exc
    matches = [port for port in list_ports.comports() if port.device == path]
    if len(matches) != 1:
        raise RuntimeError("USB_PORT_NOT_FOUND_OR_DUPLICATE")
    port = matches[0]
    expected = profiles["peers"][board]["usb_serial"]
    if (port.serial_number or "").upper() != expected or port.vid != 0x303A or port.pid != 0x1001:
        raise RuntimeError("USB_PROFILE_IDENTITY_MISMATCH")
    other = path.replace("/dev/cu.", "/dev/tty.") if path.startswith("/dev/cu.") else path.replace("/dev/tty.", "/dev/cu.")
    ownership = subprocess.run(["lsof", "-nP", "-F", "p", "--", path, other],
                               capture_output=True, text=True, timeout=3, check=False)
    if ownership.returncode == 0:
        raise RuntimeError("USB_IN_USE_BY_ANOTHER_PROCESS")
    if ownership.returncode != 1 or ownership.stderr.strip():
        raise RuntimeError("PORT_OWNERSHIP_CHECK_FAILED")


def run(command: list[str]) -> None:
    print("RUN " + " ".join(command), flush=True)
    subprocess.run(command, cwd=PROJECT, check=True)


def verify_build(board: str, build: Path, settings: dict[str, int], profiles: dict) -> Path:
    sdkconfig = build / "sdkconfig"
    actual = sdkconfig.read_text(encoding="utf-8")
    required = {
        "CONFIG_NOSTOS_PROTOCOL_V2=y",
        f"CONFIG_NOSTOS_LOCAL_SOURCE={settings['CONFIG_NOSTOS_LOCAL_SOURCE']}",
        f"CONFIG_NOSTOS_SOURCE1_ADDRESS=0x{settings['CONFIG_NOSTOS_SOURCE1_ADDRESS']:x}",
        f"CONFIG_NOSTOS_SOURCE2_ADDRESS=0x{settings['CONFIG_NOSTOS_SOURCE2_ADDRESS']:x}",
        f"CONFIG_NOSTOS_SOURCE3_ADDRESS=0x{settings['CONFIG_NOSTOS_SOURCE3_ADDRESS']:x}",
    }
    missing = sorted(line for line in required if line not in actual.splitlines())
    if missing:
        raise RuntimeError("GENERATED_CONFIG_MISMATCH: " + ", ".join(missing))
    app = build / "nostos_esp32.bin"
    if not app.is_file():
        raise RuntimeError("APP_BINARY_MISSING")
    manifest = {
        "schema": 1,
        "protocol": 2,
        "board": board,
        "target": profiles["target"],
        "source": profiles["peers"][board]["source"],
        "mesh_address": profiles["peers"][board]["mesh_address"],
        "usb_serial": profiles["peers"][board]["usb_serial"],
        "app_binary": str(app),
        "app_sha256": hashlib.sha256(app.read_bytes()).hexdigest(),
        "nvs_included": False,
    }
    (build / "nostos-v2-profile.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2), flush=True)
    return app


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description="Build or app-flash one verified NOSTOS v2 ESP32-S3 profile.")
    result.add_argument("command", choices=("show", "build", "app-flash"))
    result.add_argument("board", choices=BOARD_ORDER)
    result.add_argument("--port", help="Required for app-flash; exact verified USB callout path")
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    profiles = load_profiles()
    if args.command == "show":
        print(json.dumps(profiles["peers"][args.board], indent=2))
        return 0
    version = subprocess.run(["idf.py", "--version"], capture_output=True, text=True, check=True).stdout.strip()
    if version != "ESP-IDF v5.5.5":
        raise RuntimeError(f"ESP_IDF_VERSION_MISMATCH: {version}")
    build = PROJECT / f"build-v2-{args.board.lower()}"
    config = build / "sdkconfig"
    settings = render_sdkconfig(args.board, config, profiles)
    common = ["idf.py", "-B", str(build), "-D", f"SDKCONFIG={config}", "-D", "IDF_TARGET=esp32s3"]
    run(common + ["build"])
    verify_build(args.board, build, settings, profiles)
    if args.command == "app-flash":
        if not args.port:
            raise RuntimeError("APP_FLASH_REQUIRES_PORT")
        require_free_verified_port(args.board, args.port, profiles)
        run(common + ["-p", args.port, "app-flash"])
        print(f"APP_FLASH=PASS board={args.board} port={args.port}; NVS=NOT_WRITTEN", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"V2_PROFILE_FAILED: {exc}", file=sys.stderr)
        raise SystemExit(1)
