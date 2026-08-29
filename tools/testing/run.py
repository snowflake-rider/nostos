#!/usr/bin/env python3
"""Short human results + durable JSON/logs; default is entirely host-side."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
STAGES = {
    "pins": "핀·UART 설정 대조 (소스)",
    "host": "C/Python·메시지·출력 회귀 + ASan/UBSan (모의)",
    "protocol": "메시지·출력 회귀 (모의)",
    "stm32-debug": "STM32 Debug 전체 컴파일·링크",
    "stm32-release": "STM32 Release 전체 컴파일·링크",
    "button-output": "버튼→RGB→MP3→VS1003B 독립 검사",
    "case-tests": "ESP32 큐 단일·복합 케이스 (모의)",
    "esp32": "ESP32-S3 전체 컴파일·링크",
    "console": "Mesh Console 테스트·웹 빌드",
    "tui": "TUI 테스트·타입 검사",
    "usb": "ESP32 3대 새 STATUS 조회 (설정 일부만)",
}
# tools/test-host.sh now includes message-protocol in all three variants.
# Keep the dedicated command available, but do not run the same suite twice.
GROUPS = {"code": tuple(k for k in STAGES if k not in ("usb", "protocol")),
          "logic": ("host",),
          "stm32": ("stm32-debug", "stm32-release"),
          "apps": ("console", "tui")}
LIMITATIONS = ["배선·전압·센서·STM32 실제 출력: NOT_TESTED",
               "C000/C001 무선 송수신·다중 홉: NOT_TESTED (tests/mesh 별도)",
               "USB STATUS는 모델별 Bind/Subscription·현재 Relay 전체 검증이 아님"]


def environment():
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
    base = Path(env.get("NOSTOS_TOOLCHAINS", Path.home() / ".local/share/nostos-toolchains"))
    paths = []
    # Use already-installed project tools only. Never edit profiles or install SDKs.
    for tool, directory in (("arm-none-eabi-gcc", base / "arm-15.3-extracted/Payload/bin"),
                            ("ninja", base / "build-tools/bin")):
        if not shutil.which(tool) and directory.is_dir():
            paths.append(str(directory))
    env["PATH"] = os.pathsep.join([*paths, env.get("PATH", "")])
    if not env.get("ESP_IDF_PATH"):
        for path in (Path(env.get("IDF_PATH", "/nonexistent")), base / "esp-idf-v5.5.5",
                     Path.home() / "esp/esp-idf-v5.5.5"):
            if (path / "export.sh").is_file():
                env["ESP_IDF_PATH"] = str(path)
                break
    if "IDF_TOOLS_PATH" not in env and (base / "espressif-tools").is_dir():
        env["IDF_TOOLS_PATH"] = str(base / "espressif-tools")
    return env


def terminate(process):
    """Only signal our own process group, including compiler grandchildren."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        pass
    except ProcessLookupError:
        pass
    # The leader can exit before an uncooperative compiler/server grandchild.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def execute(command, cwd, env, log, timeout, blocked_codes=()):
    with log.open("a", encoding="utf-8") as stream:
        stream.write(f"cwd={cwd}\ncommand={json.dumps(command, ensure_ascii=False)}\n")
        stream.flush()
        try:
            process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream,
                                       stderr=subprocess.STDOUT, start_new_session=True)
        except FileNotFoundError as exc:
            stream.write(str(exc) + "\n")
            return "BLOCKED", "실행 도구 없음 — 로그의 경로 확인"
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            terminate(process)
            return "FAIL", f"제한 시간 {timeout}초 초과 — 로그 확인"
        except KeyboardInterrupt:
            terminate(process)
            raise
        if code == 0:
            return "PASS", "완료"
        if code in blocked_codes:
            return "BLOCKED", "검사 준비/해석 조건 미충족 — 로그 확인"
        if code == 2:
            # Child-specific interpretation is performed by stage(), not here.
            return "FAIL", "명령 실패 (exit=2) — 로그 확인"
        return "FAIL", f"명령 실패 (exit={code}) — 로그 확인"


def require(names, env):
    missing = [name for name in names if not shutil.which(name, path=env.get("PATH"))]
    return f"필요 도구 없음: {', '.join(missing)}" if missing else None


def failure_hint(log):
    """Surface one actionable compiler/test diagnostic, without dumping a log."""
    with log.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if re.search(r"(?:fatal )?error:|ModuleNotFoundError:|FAILED \(failures|^FAIL:|^ERROR:", line):
                line = line.strip().replace(str(ROOT) + "/", "")
                # Copied build paths are lengthy; the file:line is what matters.
                match = re.search(r"([^/\s]+:\d+(?::\d+)?: (?:fatal )?error:.*)", line)
                return (match.group(1) if match else line)[:240]
    return None


def stage(key, out, env, timeout):
    log = out / f"{key}.log"
    py = sys.executable
    commands = []
    cwd = ROOT
    needed = []
    if key == "pins":
        commands = [[py, "-B", str(ROOT / "tools/testing/check_pins.py")]]
    elif key == "host":
        needed = ["cmake", "cc"]
        env = dict(env, NOSTOS_TEST_BUILD_DIR=str(out / "host-build"), NOSTOS_PYTHON=py)
        commands = [["bash", str(ROOT / "tools/test-host.sh")]]
    elif key == "protocol":
        needed = ["cmake", "cc"]
        env = dict(env, NOSTOS_PYTHON=py)
        commands = [["bash", str(ROOT / "tests/message-protocol/run.sh")]]
    elif key.startswith("stm32-"):
        needed = ["cmake", "ninja", "arm-none-eabi-gcc", "arm-none-eabi-size"]
        target = "stm32-" + key.split("-")[1].title()
        commands = [["bash", str(ROOT / "tools/testing/build-target.sh"), target,
                     str(ROOT / "firmware/stm32"), str(out / key)]]
    elif key == "button-output":
        needed = ["cmake", "cc", "ninja", "arm-none-eabi-gcc",
                  "arm-none-eabi-size", "arm-none-eabi-nm"]
        env = dict(env, NOSTOS_BUTTON_OUTPUT_BUILD_DIR=str(out / "button-output-build"))
        commands = [["bash", str(ROOT / "tests/button-output/run.sh"), "--all"]]
    elif key == "case-tests":
        needed = ["cmake", "cc"]
        env = dict(env, NOSTOS_CASE_TEST_BUILD_DIR=str(out / "case-tests-build"))
        commands = [["bash", str(ROOT / "tests/case-tests/run.sh"), "--all"]]
    elif key == "esp32":
        needed = ["cmake", "ninja"]
        if not (Path(env.get("ESP_IDF_PATH", "/nonexistent")) / "export.sh").is_file():
            return "BLOCKED", "ESP_IDF_PATH에 설치된 ESP-IDF v5.5.5 지정", log
        expected_manifest = ["dependencies:", "example_init:",
                             "path: ${IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/example_init"]
        manifest = ROOT / "firmware/esp32/main/idf_component.yml"
        lines = [line.strip() for line in manifest.read_text().splitlines()
                 if line.strip() and not line.lstrip().startswith("#")]
        if lines != expected_manifest:
            return "BLOCKED", "ESP 의존성 변경: 호환성·설치 범위를 먼저 확인 (자동 다운로드 안 함)", log
        # A full source snapshot prevents IDF's dependency/config generation from
        # changing the checkout. Preserve relative ../../../libs/protocol paths.
        snapshot = out / "esp32-source"
        source = snapshot / "firmware/esp32"
        ignore = shutil.ignore_patterns("build*", ".git", "managed_components", "__pycache__")
        shutil.copytree(ROOT / "firmware/esp32", source, ignore=ignore)
        shutil.copytree(ROOT / "libs/protocol", snapshot / "libs/protocol", ignore=ignore)
        commands = [["bash", str(ROOT / "tools/testing/build-target.sh"), "esp32",
                     str(source), str(out / "esp32-build")]]
    elif key == "console":
        cwd = ROOT / "apps/mesh-console"
        needed = ["npm"]
        if not (cwd / ".venv/bin/python").is_file() or not (cwd / "node_modules").is_dir():
            return "BLOCKED", "Console 개발 환경 준비 필요: apps/mesh-console/README.md", log
        commands = [["bash", "scripts/test.sh"]]
    elif key == "tui":
        cwd = ROOT / "apps/esp32-tui"
        bun = cwd / "node_modules/.bin/bun"
        if not bun.is_file() or not (cwd / "node_modules/.bin/tsc").is_file():
            return "BLOCKED", "TUI 개발 환경 준비 필요: apps/esp32-tui/README.md", log
        commands = [[str(bun), "run", "typecheck"], [str(bun), "test"]]
    elif key == "usb":
        # Existing scanner is the sole transport owner and handles busy ports.
        commands = [["bash", str(ROOT / "scripts/esp32-scan"), "--ensure-console",
                     "--out", str(out / "usb.json")]]
    missing = require(needed, env)
    if missing:
        return "BLOCKED", missing, log
    state, message = "FAIL", "검사 명령 없음"
    for command in commands:
        state, message = execute(command, cwd, env, log, timeout,
                                 (2,) if key == "pins" else (78,) if key == "esp32" else ())
        if state != "PASS":
            break
    if key == "pins" and state == "PASS":
        message = "선언·초기화 대조 일치; 실제 배선은 미검증"
    if key != "usb" and state == "FAIL" and log.exists():
        message = failure_hint(log) or message
    if key == "usb":
        if state == "PASS":
            try:
                scan = json.loads((out / "usb.json").read_text())
                devices = scan["devices"]
                if (scan.get("result") != "STATUS_READ_PARTIAL_CONFIG" or len(devices) != 3
                        or {d["board"] for d in devices} != {"D6", "76", "B6"}
                        or any(d.get("status_result") != "READ" for d in devices)):
                    raise ValueError("incomplete STATUS report")
                message = " / ".join(f"{d['board']}: C001={d['firmware_report'].get('event_ready', '?')} "
                                     f"OnOff준비={d['firmware_report'].get('onoff_ready', '?')}" for d in devices)
                state = "READ"
            except (OSError, ValueError, KeyError, TypeError):
                state, message = "BLOCKED", "3대 STATUS 증거 불완전 — usb.json·로그 확인"
        elif state == "FAIL":
            state, message = "BLOCKED", "USB/서버/STATUS 확인 필요 — usb.json·로그 확인"
    return state, message, log


def exit_code(results):
    states = {row["status"] for row in results}
    if "CANCELLED" in states:
        return 130
    if "FAIL" in states:
        return 1
    if "BLOCKED" in states:
        return 2
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="NOSTOS 검사: 기본 code는 보드 없이 실행합니다.")
    parser.add_argument("stage", nargs="?", default="code", choices=[*GROUPS, *STAGES, "list"])
    parser.add_argument("--json", action="store_true", help="화면에는 최종 JSON만 출력")
    parser.add_argument("--timeout", type=int, default=1200, help="명령별 제한 초 (1..7200)")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout <= 7200:
        parser.error("--timeout: 1..7200")
    if args.stage == "list":
        for key, label in STAGES.items():
            print(f"{key:14} {label}")
        print("code = USB 제외 전체 (버튼 출력·큐 케이스 포함) / logic = 코드 회귀 / stm32 = Debug+Release / apps = Console+TUI")
        print("실물 송신은 별도: tests/mesh/README.md (--send 필수)")
        return 0
    keys = GROUPS.get(args.stage, (args.stage,))
    base = ROOT / "build/test-results"
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=datetime.now().strftime("%Y%m%d-%H%M%S-"), dir=base))
    report = {"schema": 1, "selection": args.stage, "started": datetime.now(timezone.utc).isoformat(),
              "results": [], "limitations": LIMITATIONS, "artifacts": str(out)}
    env = environment()

    def save():
        # Unique run directory; atomically publish partial results on every stage.
        temp = out / "summary.tmp"
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        temp.replace(out / "summary.json")

    save()
    for index, key in enumerate(keys):
        start = time.monotonic()
        if not args.json:
            print(f"[{index + 1}/{len(keys)}] {STAGES[key]} …", flush=True)
        try:
            status, note, log = stage(key, out, env, args.timeout)
        except KeyboardInterrupt:
            status, note, log = "CANCELLED", "사용자 중단; 추가 검사 없음", out / f"{key}.log"
        except Exception as exc:
            status, note, log = "FAIL", f"검사기 오류: {type(exc).__name__}: {exc}", out / f"{key}.log"
        row = {"id": key, "label": STAGES[key], "status": status, "note": note,
               "seconds": round(time.monotonic() - start, 1), "log": str(log) if log.exists() else None}
        report["results"].append(row)
        save()
        if not args.json:
            print(f"  {status:9} {note} ({row['seconds']}초)", flush=True)
        if status == "CANCELLED":
            for remaining in keys[index + 1:]:
                report["results"].append({"id": remaining, "label": STAGES[remaining],
                                          "status": "NOT_RUN", "note": "앞 단계에서 중단"})
            break
    report["exit_code"] = exit_code(report["results"])
    report["ended"] = datetime.now(timezone.utc).isoformat()
    save()
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        counts = {state: sum(row["status"] == state for row in report["results"])
                  for state in ("PASS", "READ", "FAIL", "BLOCKED", "CANCELLED", "NOT_RUN")}
        print("\n요약: " + " / ".join(f"{k} {v}" for k, v in counts.items() if v))
        print("실물 배선·센서·UART·Mesh·다중 홉: NOT_TESTED")
        print(f"상세: {out / 'summary.json'}")
    return report["exit_code"]


if __name__ == "__main__":
    sys.exit(main())
