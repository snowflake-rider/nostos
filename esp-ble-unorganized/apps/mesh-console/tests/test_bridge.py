import asyncio
import time
from types import SimpleNamespace
from unittest.mock import patch

import pytest
import serial

from server.bridge import Bridge, BridgeError, NoControlSerial, open_serial
from server.protocol import LineDecoder, parse_line
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
