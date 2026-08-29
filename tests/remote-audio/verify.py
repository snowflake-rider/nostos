#!/usr/bin/env python3
"""Verify one NOSTOS remote button-to-audio trial from captured evidence."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys
from typing import Any


EVENT_IDS = {
    "speed-down": 0x10,
    "speed-up": 0x11,
    "stop": 0x13,
}
STAGE_RE = re.compile(
    r"\b(UART_RX|MESH_TX|MESH_RX|UART_TX)\b.*?\bid=0x([0-9a-fA-F]{1,2})\b"
)
FIELD_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)=([^\s;]+)")
FORBIDDEN_IDS = {0x30, 0x31}


class EvidenceError(ValueError):
    """Raised when an evidence document is structurally invalid."""


def parse_integer(value: Any, field: str) -> int:
    if isinstance(value, bool):
        raise EvidenceError(f"{field} must be an integer, not boolean")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as exc:
            raise EvidenceError(f"{field} is not an integer: {value!r}") from exc
    raise EvidenceError(f"{field} must be an integer")


def require_mapping(value: Any, field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceError(f"{field} must be an object")
    return value


def require_lines(board: dict[str, Any], field: str) -> list[str]:
    lines = board.get("log")
    if not isinstance(lines, list) or not all(isinstance(line, str) for line in lines):
        raise EvidenceError(f"{field}.log must be an array of strings")
    return lines


def parse_stages(lines: list[str]) -> list[tuple[int, str, int, dict[str, str]]]:
    parsed = []
    for index, line in enumerate(lines):
        match = STAGE_RE.search(line)
        if match:
            parsed.append(
                (index, match.group(1), int(match.group(2), 16), dict(FIELD_RE.findall(line)))
            )
    return parsed


def stage_accepted(stage: str, fields: dict[str, str]) -> bool:
    if stage.endswith("RX"):
        return fields.get("result") == "queued"
    return fields.get("api") == "accepted"


def check_esp_path(
    source: dict[str, Any], receiver: dict[str, Any], event_id: int
) -> tuple[dict[str, int], list[str]]:
    source_name = str(source.get("name", "source"))
    receiver_name = str(receiver.get("name", "receiver"))
    source_address = parse_integer(source.get("primary"), "source.primary")
    receiver_address = parse_integer(receiver.get("primary"), "receiver.primary")
    if source_address == receiver_address:
        raise EvidenceError("source.primary and receiver.primary must differ")

    parsed = {
        source_name: parse_stages(require_lines(source, "source")),
        receiver_name: parse_stages(require_lines(receiver, "receiver")),
    }
    expected = {
        (source_name, "UART_RX"): None,
        (source_name, "MESH_TX"): source_address,
        (receiver_name, "MESH_RX"): source_address,
        (receiver_name, "UART_TX"): source_address,
    }
    counts: Counter[str] = Counter()
    problems: list[str] = []

    for board_name, stages in parsed.items():
        sent_to_uart_at: dict[int, int] = {}
        for index, stage, ident, fields in stages:
            if ident in FORBIDDEN_IDS:
                problems.append(f"FORBIDDEN_SAFETY_EVENT:{board_name}:{stage}:0x{ident:02x}")
            if stage == "UART_TX":
                sent_to_uart_at[ident] = index
            elif stage == "UART_RX" and ident in sent_to_uart_at and index > sent_to_uart_at[ident]:
                problems.append(f"UART_ECHO_LOOP:{board_name}:0x{ident:02x}")

            key = (board_name, stage)
            if ident != event_id or key not in expected:
                continue
            label = f"{board_name}.{stage}"
            counts[label] += 1
            if not stage_accepted(stage, fields):
                problems.append(f"NOT_ACCEPTED:{label}")
            expected_source = expected[key]
            if expected_source is not None:
                actual_source = fields.get("source")
                try:
                    parsed_source = int(actual_source, 0) if actual_source is not None else None
                except ValueError:
                    parsed_source = None
                if parsed_source != expected_source:
                    problems.append(f"WRONG_SOURCE:{label}")

    expected_labels = [f"{board}.{stage}" for board, stage in expected]
    for label in expected_labels:
        count = counts[label]
        if count == 0:
            problems.append(f"MISSING_STAGE:{label}")
        elif count > 1:
            problems.append(f"DUPLICATE_STAGE:{label}:{count}")

    return {label: counts[label] for label in expected_labels}, problems


def check_stm32(
    stm32: dict[str, Any] | None, event_id: int
) -> tuple[str, dict[str, Any], list[str]]:
    if stm32 is None:
        return "INCOMPLETE", {}, ["STM32_EVIDENCE_MISSING"]

    rx_before = parse_integer(stm32.get("rx_count_before"), "receiver_stm32.rx_count_before")
    rx_after = parse_integer(stm32.get("rx_count_after"), "receiver_stm32.rx_count_after")
    remote_before = parse_integer(
        stm32.get("remote_count_before"), "receiver_stm32.remote_count_before"
    )
    remote_after = parse_integer(
        stm32.get("remote_count_after"), "receiver_stm32.remote_count_after"
    )
    last_received = parse_integer(
        stm32.get("last_received"), "receiver_stm32.last_received"
    )
    audio_status = parse_integer(stm32.get("audio_status"), "receiver_stm32.audio_status")
    observations = {
        "audio_heard": stm32.get("audio_heard"),
        "buzzer_heard": stm32.get("buzzer_heard"),
        "rgb_changed": stm32.get("rgb_changed"),
    }
    if any(not isinstance(value, bool) for value in observations.values()):
        raise EvidenceError(
            "receiver_stm32 audio_heard, buzzer_heard and rgb_changed must be booleans"
        )

    problems = []
    if rx_after - rx_before != 1:
        problems.append(f"STM32_RX_DELTA:{rx_after - rx_before}")
    if remote_after - remote_before != 1:
        problems.append(f"STM32_REMOTE_DELTA:{remote_after - remote_before}")
    if last_received != event_id:
        problems.append(f"STM32_LAST_RECEIVED:0x{last_received:02x}")
    if audio_status != 0:
        problems.append(f"STM32_AUDIO_STATUS:{audio_status}")
    if not observations["audio_heard"]:
        problems.append("REMOTE_AUDIO_NOT_HEARD")
    if observations["buzzer_heard"]:
        problems.append("BUTTON_TRIGGERED_BUZZER")
    if observations["rgb_changed"]:
        problems.append("REMOTE_BUTTON_CHANGED_RGB")

    details = {
        "rx_delta": rx_after - rx_before,
        "remote_delta": remote_after - remote_before,
        "last_received": f"0x{last_received:02x}",
        "audio_status": audio_status,
        **observations,
    }
    return ("FAIL" if problems else "PASS"), details, problems


def verify(evidence: dict[str, Any]) -> dict[str, Any]:
    event_name = evidence.get("event")
    if event_name not in EVENT_IDS:
        raise EvidenceError(f"event must be one of: {', '.join(EVENT_IDS)}")
    event_id = EVENT_IDS[event_name]
    source = require_mapping(evidence.get("source"), "source")
    receiver = require_mapping(evidence.get("receiver"), "receiver")
    stm32_value = evidence.get("receiver_stm32")
    stm32 = None if stm32_value is None else require_mapping(stm32_value, "receiver_stm32")

    stages, esp_problems = check_esp_path(source, receiver, event_id)
    stm32_verdict, stm32_details, stm32_problems = check_stm32(stm32, event_id)
    problems = sorted(set(esp_problems + stm32_problems))
    if esp_problems or stm32_verdict == "FAIL":
        verdict = "FAIL"
    elif stm32_verdict == "INCOMPLETE":
        verdict = "INCOMPLETE"
    else:
        verdict = "PASS"
    return {
        "verdict": verdict,
        "event": event_name,
        "event_id": f"0x{event_id:02x}",
        "stages": stages,
        "receiver_stm32": stm32_details,
        "problems": problems,
    }


def print_human(report: dict[str, Any]) -> None:
    print(f"[{report['verdict']}] {report['event']} ({report['event_id']})")
    for stage, count in report["stages"].items():
        marker = "PASS" if count == 1 else "FAIL"
        print(f"  {marker:4} {stage}: {count}")
    if report["receiver_stm32"]:
        print("  STM32  " + json.dumps(report["receiver_stm32"], ensure_ascii=False))
    for problem in report["problems"]:
        print(f"  - {problem}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify one button -> Mesh -> remote STM32 audio evidence document."
    )
    parser.add_argument("evidence", type=Path, help="JSON evidence file")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args(argv)
    try:
        evidence = json.loads(args.evidence.read_text(encoding="utf-8"))
        if not isinstance(evidence, dict):
            raise EvidenceError("top-level JSON value must be an object")
        report = verify(evidence)
    except (OSError, json.JSONDecodeError, EvidenceError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    else:
        print_human(report)
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
