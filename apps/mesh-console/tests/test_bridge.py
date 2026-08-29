import asyncio
import time
import json
import re
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest
import serial

from server.bridge import Bridge, BridgeError, NoControlSerial, open_serial
from server.protocol import EVENT_NAMES, LineDecoder, parse_line
from server.log_store import LogStore
from .fakes import FakeUSB, status_line


@pytest.fixture
def anyio_backend():
    return "asyncio"


@pytest.fixture
def rig():
    usb = FakeUSB()
    bridge = Bridge(usb.scan, usb.open, mode="test")
    bridge.subscribers.add(asyncio.Queue(maxsize=256))
    return bridge, usb


def test_decoder_fragmented_utf8_crlf_ansi_and_bound():
    decoder = LineDecoder()
    raw = "\x1b[32m한글\x1b[0m\r\nnext\n".encode()
    lines = []
    for byte in raw:
        lines.extend(decoder.feed(bytes([byte])))
    assert lines == ["한글", "next"]
    assert len(decoder.feed(b"x" * 9000)) == 1
    assert not decoder.pending
    assert decoder.feed(b"tail\nokay\n") == ["okay"]


@pytest.mark.parametrize("text,category,level", [
    ("I (20) X: UART_RX id=1 result=queued", "UART RX", "info"),
    ("I (20) X: MESH_TX id=1 source=2 api=accepted", "MESH TX", "info"),
    ("I (20) X: MESH_RX source=2 id=1 result=queued", "MESH RX", "info"),
    ("I (20) X: UART_TX id=1 source=2 api=accepted", "UART TX", "info"),
    ("I (20) NOSTOS_V2: UART_RX type=0x13 source=2 len=9 result=OK", "UART RX", "info"),
    ("I (20) NOSTOS_V2: MESH_TX type=0x13 source=2 len=9 result=OK api=accepted", "MESH TX", "info"),
    ("I (20) NOSTOS_V2: MESH_RX type=0x13 source=1 address=0x0003 len=9 result=OK", "MESH RX", "info"),
    ("I (20) NOSTOS_V2: UART_TX type=0x13 source=1 len=9 result=OK api=accepted", "UART TX", "info"),
    ("I (20) NOSTOS_V2: UART_RX type=0x13 source=2 len=9 result=FULL", "ERROR", "error"),
    ("I (20) NOSTOS_V2: MESH_RX type=0x13 source=1 address=0x0003 len=9 result=UNAUTHORIZED", "ERROR", "error"),
    ("I (20) NOSTOS_V2: UART_TX type=0x13 source=1 len=9 result=OK api=failed", "ERROR", "error"),
    ("E (20) X: failure", "ERROR", "error"),
    ("I (20) X: MESH_TX id=1 api=failed", "ERROR", "error"),
    ("W (20) X: warning", "OTHER", "warning"),
    ("unknown <script>text</script>", "OTHER", "info"),
    ("[LAYER-8] BOOT_START", "SYSTEM", "info"),
])
def test_categories_preserve_acceptance_distinction(text, category, level):
    parsed = parse_line(text)
    assert (parsed["category"], parsed["level"]) == (category, level)
    assert parsed["status"] is None


def test_status_requires_complete_numeric_fields():
    assert parse_line(status_line())["status"]["sub_C001"] == "1"
    assert parse_line(status_line().replace(" ttl=7", ""))["status"] is None
    assert parse_line(status_line().replace("ttl=7", "ttl=no"))["status"] is None


def test_scan_identity_and_never_opens_at_start(rig):
    bridge, usb = rig
    bridge.refresh()
    assert [n["detected"] for n in bridge.snapshot()["nodes"]] == [True] * 3
    assert usb.opened == []
    ports = usb.scan()
    ports[0].vid = 0
    ports.append(SimpleNamespace(device="/fake/STM32", serial_number="unknown", vid=0x483, pid=0x374b))
    bridge.scanner = lambda: ports
    bridge.refresh()
    assert bridge.nodes["D6"].path is None
    assert len(bridge.nodes) == 3


@pytest.mark.anyio
async def test_connect_needs_ui_and_duplicate_connect_owns_one_handle(rig):
    bridge, usb = rig
    bridge.subscribers.clear()
    with pytest.raises(BridgeError):
        await bridge.connect("D6")
    bridge.subscribers.add(asyncio.Queue())
    await asyncio.gather(bridge.connect("D6"), bridge.connect("D6"))
    assert len(usb.opened) == 1
    assert usb.opened[0].writes == [b"status\n"]
    assert bridge.snapshot()["nodes"][0]["fresh"] is False
    bridge.log("D6", status_line())
    assert bridge.snapshot()["nodes"][0]["fresh"] is True
    await bridge.disconnect("D6")
    assert usb.opened[0].closed
    assert bridge.snapshot()["nodes"][0]["status"] is None


@pytest.mark.anyio
async def test_commands_require_current_matching_firmware_and_onoff_model(rig):
    bridge, usb = rig
    await bridge.connect("D6")
    for command in ["factory-reset", "on\noff", "unknown", "on"]:
        with pytest.raises(BridgeError):
            await bridge.command("D6", command)
    bridge.log("D6", status_line("B6"))
    assert not bridge.nodes["D6"].status
    bridge.log("D6", status_line(ready=0))
    with pytest.raises(BridgeError, match="C000"):
        await bridge.command("D6", "on")
    bridge.log("D6", status_line())
    for command in ["on", "off", "on-unack", "off-unack", "tx-normal", "tx-low", "status"]:
        bridge.nodes["D6"].command_at = -100
        assert (await bridge.command("D6", command))["result"] == "written"
    assert usb.opened[0].writes[-1] == b"status\n"
    bridge.nodes["D6"].status_mono -= 16
    assert not bridge.snapshot()["nodes"][0]["fresh"]
    with pytest.raises(BridgeError, match="최신"):
        await bridge.command("D6", "off")
    bridge.log("D6", "[LAYER-8] BOOT_START")
    assert bridge.nodes["D6"].status is None


@pytest.mark.anyio
async def test_rate_limit_and_partial_write_no_retry(rig):
    bridge, usb = rig
    await bridge.connect("D6")
    bridge.log("D6", status_line())
    await bridge.command("D6", "on")
    with pytest.raises(BridgeError) as error:
        await bridge.command("D6", "off")
    assert error.value.status == 429
    bridge.nodes["D6"].command_at = -100
    usb.opened[0].partial_write = True
    with pytest.raises(BridgeError, match="자동 재시도"):
        await bridge.command("D6", "off")
    assert usb.opened[0].closed
    assert usb.opened[0].writes.count(b"off\n") == 1
    assert bridge.nodes["D6"].port is None


@pytest.mark.anyio
async def test_loop_read_unplug_browser_close_and_shutdown(rig):
    bridge, usb = rig
    await bridge.start()
    try:
        await bridge.connect("D6")
        for _ in range(20):
            if bridge.nodes["D6"].status:
                break
            await asyncio.sleep(0.02)
        assert bridge.nodes["D6"].status
        usb.present.remove("D6")
        bridge.refresh()
        await asyncio.sleep(0.08)
        assert usb.opened[0].closed
        assert bridge.nodes["D6"].phase == "error"
        await bridge.connect("76")
        bridge.subscribers.clear()
        bridge.empty_since = time.monotonic() - 3
        await asyncio.sleep(0.08)
        assert usb.opened[1].closed
        assert bridge.nodes["76"].phase == "disconnected"
    finally:
        await bridge.stop()
    assert all(p.closed for p in usb.opened)


def test_buffer_bounds_and_slow_subscriber_signal(rig):
    bridge, _ = rig
    queue = asyncio.Queue(maxsize=2)
    bridge.subscribers = {queue}
    for _ in range(5100):
        bridge.log("D6", "sample")
    assert len(bridge.logs) == 5000 and bridge.dropped == 100
    assert queue.get_nowait() == {"type": "overflow"}
    bridge.logs.clear()
    bridge.log_bytes = 0
    for _ in range(700):
        bridge.log("D6", "가" * 2600)
    assert bridge.log_bytes <= 5 * 1024 * 1024


def test_port_configuration_never_calls_modem_control():
    with patch("server.bridge.NoControlSerial") as factory:
        open_serial("/fake/D6")
        args, kwargs = factory.call_args
        assert args == ("/fake/D6", 115200)
        assert kwargs["exclusive"] and kwargs["timeout"] == 0
        assert not kwargs["rtscts"] and not kwargs["dsrdtr"]
    with patch.object(serial.Serial, "_update_dtr_state") as dtr, patch.object(serial.Serial, "_update_rts_state") as rts:
        NoControlSerial._update_dtr_state(None)
        NoControlSerial._update_rts_state(None)
        dtr.assert_not_called()
        rts.assert_not_called()


def test_message_interpretation_matches_c_contract_and_preserves_outcome():
    contract = (Path(__file__).resolve().parents[3] / "libs/protocol/message_type.h").read_text()
    ids = {name: int(value, 16) for name, value in re.findall(r"(MSG_\w+)\s*=\s*(0x[\dA-Fa-f]+)", contract)}
    v2_contract = (Path(__file__).resolve().parents[3] / "libs/protocol/nostos_protocol.h").read_text()
    v2_ids = {name: int(value, 16) for name, value in re.findall(r"(NOSTOS_\w+)\s*=\s*(0x[\dA-Fa-f]+)", v2_contract)}
    expected = {value for name, value in ids.items() if name not in {"MSG_NONE", "MSG_UNKNOWN"}}
    expected.update(v2_ids.values())
    assert set(EVENT_NAMES) == expected
    for event_id, (symbol, label) in EVENT_NAMES.items():
        assert ids.get(symbol, v2_ids.get(symbol)) == event_id
        for marker in ("UART_RX", "MESH_TX", "MESH_RX", "UART_TX"):
            text = f"I (100) LAYER_8_UART: {marker} source=0x0003 id=0x{event_id:02x} api=failed"
            parsed = parse_line(text)
            assert parsed["event"]["label"] == label
            assert parsed["event"]["source"] == "0x0003"
            assert parsed["event"]["result"] == "failed"
            assert parsed["category"] == "ERROR"  # Interpretation is not success.
    assert parse_line("MESH_RX source=3 id=0x99 result=queued")["event"]["known"] is False
    v2=parse_line("I (100) NOSTOS_V2: MESH_RX type=0x13 source=1 address=0x0003 len=9 result=OK")
    assert v2["event"]["hex"] == "0x13"
    assert v2["event"]["source"] == "1"
    assert v2["event"]["result"] == "OK"
    assert v2["category"] == "MESH RX"
    final_failure=parse_line("I (100) NOSTOS_V2: UART_TX type=0x51 source=1 len=18 result=OK api=failed")
    assert final_failure["event"]["result"] == "failed"
    assert final_failure["category"] == "ERROR"
    for text in ["STATUS id=0x13", "free text id=0x13", "MESH_TX id=0x130", "MESH_TX id=256", "MESH_TX id=0x13junk"]:
        assert parse_line(text)["event"] is None
    assert parse_line("ONOFF_RX src=0x0005 value=1")["category"] == "ONOFF RX"


@pytest.mark.anyio
async def test_archive_keeps_more_than_screen_limit_and_survives_restart(tmp_path):
    usb = FakeUSB()
    bridge = Bridge(usb.scan, usb.open, "test", log_directory=tmp_path)
    await bridge.start()
    text = "I (100) LAYER_8_UART: MESH_RX source=0x0003 id=0x13 result=queued 한글"
    try:
        for index in range(5101):
            bridge.log("D6", text)
            if index % 100 == 0:
                while bridge.archive.pending:
                    await asyncio.sleep(0.001)
        assert len(bridge.logs) == 5000 and bridge.dropped == 101
    finally:
        await bridge.stop()
    old = bridge.archive.path
    records = [json.loads(line) for line in old.read_text(encoding="utf-8").splitlines()]
    assert len(records) == bridge.archive.saved == 5101
    assert bridge.archive.pending == bridge.archive.missed == 0
    assert records[-1]["event"]["label"] == "정지 요청"
    assert records[-1]["text"] == text
    assert {r["mode"] for r in records} == {"test"}
    assert "정지 요청".encode() in old.read_bytes()
    assert old.stat().st_mode & 0o777 == 0o600
    again = Bridge(usb.scan, usb.open, "test", log_directory=tmp_path)
    await again.start()
    again.log("D6", "new session")
    await again.stop()
    assert again.archive.path != old
    assert len(list(tmp_path.glob("*.jsonl"))) == 2
    assert len(old.read_text().splitlines()) == 5101
    assert usb.opened == []


@pytest.mark.anyio
async def test_archive_open_write_and_backpressure_errors_are_visible(tmp_path):
    blocked = tmp_path / "not-a-directory"
    blocked.write_text("keep")
    store = LogStore(blocked, "open-fail", "test")
    await store.start()
    store.append({"text": "still visible in memory"})
    assert store.error and store.missed == 1
    await store.stop()
    store = LogStore(tmp_path, "write-fail", "test")
    await store.start()
    with patch.object(store, "_write", side_effect=OSError("disk full")):
        store.append({"text": "한글"})
        await store.stop()
    assert "disk full" in store.snapshot()["error"]
    assert store.saved == 0 and store.missed == 1 and store.pending == 0
    store = LogStore(tmp_path, "queue-full", "test")
    await store.start()
    store.append({"text": "first"})
    with patch("server.log_store.QUEUE_BYTES", 1):
        store.append({"text": "overflow"})
    await store.stop()
    assert store.saved == 1 and store.missed == 1 and store.pending == 0
    assert "한도" in store.error
