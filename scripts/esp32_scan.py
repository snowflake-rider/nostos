#!/usr/bin/env python3
"""Read USB identity + fresh STATUS for D6/76/B6; never claim a full Mesh config dump."""
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("nostos_mesh_repeat", ROOT / "tools/hardware/mesh_repeat.py")
mesh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mesh)
console_spec = importlib.util.spec_from_file_location("nostos_esp32_console", ROOT / "scripts/esp32_console.py")
console_helper = importlib.util.module_from_spec(console_spec)
console_spec.loader.exec_module(console_helper)
BOARDS = ("D6", "76", "B6")
FIELDS = ("name", "primary", "net", "app", "pub", "sub_C001", "event_ready",
          "ttl", "period", "retransmit", "onoff_ready", "state", "relay")
UNAVAILABLE = (
    "configuration_snapshot_updated_at",
    "model_appkey_bindings",
    "onoff_client_publication_address_and_key_index",
    "onoff_client_publication_ttl_period_retransmit",
    "onoff_server_subscriptions_and_bindings",
    "relay_current_state_and_retransmit",
    "proxy_friend_lpn_features_and_default_ttl",
)
LIMITATIONS = [
    "STATUS is a firmware-cached snapshot; a new USB response is not a new Mesh configuration readback.",
    "pub/net/app/ttl/period/retransmit/sub_C001 describe the Vendor C001 model, not Generic OnOff.",
    "onoff_ready is Client readiness only; it does not verify receiver Server configuration.",
    "relay_cached is not a reliable current Relay readback; publication retransmit is not Relay retransmit.",
    "No Mesh key bytes, settings writes, ON/OFF test transmissions, flash, reset or RF success claims.",
]


def inventory(ports):
    devices = []
    for board in BOARDS:
        matches = [p for p in ports if (p.serial_number or "").upper() == mesh.IDENTITIES[board]
                   and p.vid == 0x303A and p.pid == 0x1001]
        path = matches[0].device if len(matches) == 1 else None
        error = "USB_NOT_FOUND" if not matches else "DUPLICATE_USB_IDENTITY" if len(matches) > 1 else None
        if path and not re.fullmatch(r"/dev/(?:cu|tty)\.usbmodem[A-Za-z0-9_-]+", path):
            error, path = "UNEXPECTED_USB_PATH", None
        devices.append({"board": board, "usb_serial": mesh.IDENTITIES[board], "usb_path": path,
                        "usb_matches": len(matches), "status_received_at": None,
                        "status_result": error or "NOT_READ", "firmware_report": None})
    return devices


def require_free_port(path):
    """Do not steal from another monitor; check both macOS callout and dial-in aliases."""
    other = path.replace("/dev/cu.", "/dev/tty.") if path.startswith("/dev/cu.") else path.replace("/dev/tty.", "/dev/cu.")
    try:
        result = subprocess.run(["lsof", "-nP", "-F", "p", "--", path, other],
                                capture_output=True, text=True, timeout=3)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError("PORT_OWNERSHIP_CHECK_UNAVAILABLE") from exc
    if result.returncode == 0:
        raise RuntimeError("USB_IN_USE_BY_ANOTHER_PROCESS")
    if result.returncode != 1 or result.stderr.strip():
        raise RuntimeError("PORT_OWNERSHIP_CHECK_FAILED")


def node_for(state, device):
    matches = [n for n in state.get("nodes", []) if n.get("board") == device["board"]]
    if len(matches) != 1:
        raise ValueError("CONSOLE_NODE_MISSING_OR_DUPLICATE")
    node = matches[0]
    if node.get("serial") != device["usb_serial"] or node.get("path") != device["usb_path"]:
        raise ValueError("CONSOLE_USB_IDENTITY_MISMATCH")
    return node


def checked_state(state, instance=None):
    if state.get("mode") != "live":
        raise RuntimeError("CONSOLE_NOT_LIVE")
    if state.get("scan_error"):
        raise RuntimeError("CONSOLE_USB_SCAN_ERROR")
    if not state.get("instance") or (instance and state["instance"] != instance):
        raise RuntimeError("CONSOLE_INSTANCE_CHANGED")


def safe_status(status, board):
    """Known scalar fields only; never echo arbitrary device logs, fields or key material."""
    if not isinstance(status, dict) or status.get("name") != mesh.NAMES[board]:
        raise ValueError("FIRMWARE_IDENTITY_MISMATCH")
    result = {"name": status["name"]}
    for key in FIELDS[1:]:
        value = status.get(key)
        if not isinstance(value, str) or not re.fullmatch(r"(?:0[xX][0-9a-fA-F]{1,4}|[0-9]{1,5})", value):
            raise ValueError("INCOMPLETE_OR_INVALID_STATUS")
        number = int(value, 16 if value.lower().startswith("0x") else 10)
        maximum = 0xFFFF if key in ("primary", "pub", "net", "app") else 255
        if key in ("sub_C001", "event_ready", "onoff_ready", "state"):
            maximum = 1
        elif key == "relay":
            maximum = 2
        if number > maximum:
            raise ValueError("INCOMPLETE_OR_INVALID_STATUS")
        result["relay_cached" if key == "relay" else key] = value
    return result


def accept_status(device, node, since, now):
    if node.get("error") or node.get("phase") == "error":
        raise ValueError("DEVICE_OR_CONSOLE_ERROR")
    if node.get("phase") != "connected" or node.get("fresh") is not True:
        return False
    try:
        at = float(node["status_at"]) / 1000
    except (KeyError, TypeError, ValueError):
        return False
    if not math.isfinite(at) or at < since or not -1 <= now - at < 15:
        return False
    report = safe_status(node.get("status"), device["board"])
    device.update(status_received_at=datetime.fromtimestamp(at, timezone.utc).isoformat(),
                  status_result="READ", firmware_report=report)
    return True


def read_console(devices, console, timeout, connect_missing=True, port_guard=require_free_port):
    pending = [d for d in devices if d["status_result"] == "NOT_READ"]
    if not pending:
        return
    # A subscriber lets the existing server own/poll USB. No second serial reader.
    with console.stream() as stream:
        first = json.loads(stream.recv(timeout=3))
        if first.get("type") != "snapshot":
            raise RuntimeError("CONSOLE_SNAPSHOT_MISSING")
        state = first["state"]
        checked_state(state)
        instance = state["instance"]
        since = time.time()
        deadline = time.monotonic() + timeout
        for device in list(pending):
            try:
                node = node_for(state, device)
                phase = node.get("phase")
                if phase == "disconnected":
                    if not connect_missing:
                        raise ValueError("CONSOLE_NOT_CONNECTED")
                    try:
                        port_guard(device["usb_path"])
                    except RuntimeError as busy:
                        # Another query may have connected this board since our
                        # snapshot. Reuse only the same identified Console's handle.
                        shared = False
                        if str(busy) == "USB_IN_USE_BY_ANOTHER_PROCESS":
                            try:
                                latest = console.request("/api/state")
                                checked_state(latest, instance)
                                shared = node_for(latest, device).get("phase") in ("connected", "verifying")
                            except Exception:
                                pass
                        if shared:
                            continue
                        raise
                    # The only POST this tool makes: open the known board for STATUS.
                    console.request("/api/boards/" + device["board"] + "/connect", {})
                elif phase not in ("connected", "verifying"):
                    raise ValueError("CONSOLE_BUSY_OR_ERROR")
            except (ValueError, RuntimeError) as exc:
                code = str(exc)
                device["status_result"] = code if re.fullmatch(r"[A-Z_]+", code) else "CONSOLE_CONNECT_FAILED"
                pending.remove(device)
        while pending and time.monotonic() < deadline:
            try:
                event = json.loads(stream.recv(timeout=min(1, max(.01, deadline - time.monotonic()))))
            except TimeoutError:
                continue
            if event.get("type") == "log":
                continue  # Never persist/print raw logs, including the historical snapshot logs.
            if event.get("type") != "state":
                raise RuntimeError("CONSOLE_STREAM_INTERRUPTED")
            checked_state(event, instance)
            for device in list(pending):
                try:
                    if accept_status(device, node_for(event, device), since, time.time()):
                        pending.remove(device)
                except ValueError as exc:
                    device["status_result"] = str(exc)
                    pending.remove(device)
        for device in pending:
            device["status_result"] = "STATUS_TIMEOUT"


def scan(ports, console, timeout=10, usb_only=False, connect_missing=True, port_guard=require_free_port,
         prepare_console=None):
    devices = inventory(ports)
    error = None
    if not usb_only:
        try:
            if prepare_console and connect_missing and any(d["status_result"] == "NOT_READ" for d in devices):
                prepare_console()
            read_console(devices, console, timeout, connect_missing, port_guard)
        except KeyboardInterrupt:
            error = "INTERRUPTED"
        except Exception as exc:
            # HTTP errors can contain arbitrary response content: never echo it.
            code = str(exc)
            error = code if isinstance(exc, RuntimeError) and re.fullmatch(r"[A-Z_]+", code) else "CONSOLE_UNAVAILABLE_OR_STREAM_ERROR"
        if error:
            for device in devices:
                if device["status_result"] == "NOT_READ":
                    device["status_result"] = error
    complete = all(d["usb_matches"] == 1 and d["usb_path"] for d in devices) if usb_only else all(d["status_result"] == "READ" for d in devices)
    return {"schema": 1, "checked_at": mesh.utc(), "result": "USB_ONLY" if usb_only and complete else
            "STATUS_READ_PARTIAL_CONFIG" if complete and not error else "INCOMPLETE",
            "backend": "usb-enumeration" if usb_only else "mesh-console",
            "error": error, "devices": devices,
            "configuration_snapshot_updated_at": None, "unavailable_fields": list(UNAVAILABLE),
            "limitations": LIMITATIONS, "mesh_test_transmissions_sent": 0}


def display(report):
    print(f"ESP32 SCAN  {report['checked_at']}  [{report['result']}]")
    print("보드  USB 포트               주소     C001 Pub  이벤트준비  OnOff송신준비  Relay(캐시)")
    for device in report["devices"]:
        s = device["firmware_report"] or {}
        path = (device["usb_path"] or "미감지").removeprefix("/dev/")
        print(f"{device['board']:4} {path:22} {s.get('primary', '?'):8} {s.get('pub', '?'):9} "
              f"{s.get('event_ready', '?'):11} {s.get('onoff_ready', '?'):13} {s.get('relay_cached', '?')}")
        if s:
            print(f"     C001: NetIdx={s['net']} AppIdx={s['app']} 구독={s['sub_C001']} "
                  f"TTL={s['ttl']} Period={s['period']} Publication재전송={s['retransmit']} | OnOff상태={s['state']}")
        elif report["backend"] != "usb-enumeration":
            print("     조회 결과: " + device["status_result"])
    print("\n준비 열: 1=준비 보고, 0=미준비 보고. ?=조회 불가. 무선 성공 판정이 아닙니다.")
    print("위 값은 펌웨어 캐시입니다. 새 STATUS 수신과 Mesh 설정 재조회는 다릅니다.")
    print("조회 불가: 모델별 실제 Bind 목록, C000 Publication 상세/Server 구독, 현재 Relay·재전송.")
    if report["error"]:
        print("오류: " + report["error"] + " — Mesh Console 실행/연결 상태를 확인하세요.")


def parser():
    p = argparse.ArgumentParser(description="ESP32 D6/76/B6 USB 및 Mesh STATUS 조회. 설정 변경/시험 송신 없음.")
    p.add_argument("--json", action="store_true", help="키 원문 없는 JSON 출력")
    p.add_argument("--usb-only", action="store_true", help="USB 식별만; 포트/Console 연결 없음")
    p.add_argument("--no-connect", action="store_true", help="이미 Console에 연결된 보드만 읽기")
    p.add_argument("--ensure-console", action="store_true", help="필요하면 내부 Console 자동 시작; 사용하지 않으면 유휴 종료")
    p.add_argument("--port", type=int, default=8787, help="기존 로컬 Mesh Console 포트")
    p.add_argument("--timeout", type=float, default=10, help="상태 대기 제한, 1..30초")
    p.add_argument("--out", type=Path, help="결과 JSON을 새 파일에 저장; 기존 파일 덮어쓰기 금지")
    return p


def main(argv=None):
    p = parser()
    args = p.parse_args(argv)
    if not 1 <= args.port <= 65535 or not math.isfinite(args.timeout) or not 1 <= args.timeout <= 30:
        p.error("port는 1..65535, timeout은 1..30초여야 합니다")
    if args.out and args.out.exists():
        p.error("--out 파일이 이미 존재합니다. 새 경로를 지정하세요")
    try:
        from serial.tools import list_ports
        ports = list(list_ports.comports())  # Enumeration only: never opens a serial port.
    except Exception:
        print("USB_SCAN_UNAVAILABLE: Console의 Python 환경/pyserial을 확인하세요.", file=sys.stderr)
        return 2
    report = scan(ports, mesh.Console(args.port), args.timeout, args.usb_only, not args.no_connect,
                  prepare_console=(lambda: console_helper.ensure_console(args.port)) if args.ensure_console else None)
    data = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.out:
        try:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            with args.out.open("x", encoding="utf-8") as output:
                output.write(data)
        except OSError:
            print("REPORT_WRITE_FAILED: 결과를 저장하지 못했습니다.", file=sys.stderr)
            return 2
    print(data, end="") if args.json else display(report)
    if args.out:
        print("REPORT " + str(args.out.resolve()), file=sys.stderr)
    return 2 if report["result"] == "INCOMPLETE" else 0


if __name__ == "__main__":
    sys.exit(main())
