import pytest
import json
import time
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect
from server.app import create_app
from server.bridge import Bridge
from .fakes import FakeUSB

ORIGIN = {"origin": "http://127.0.0.1:8787"}


@pytest.fixture
def client(tmp_path):
    usb = FakeUSB()
    bridge = Bridge(usb.scan, usb.open, "test", log_directory=tmp_path)
    with TestClient(create_app(bridge), base_url="http://127.0.0.1:8787") as client:
        yield client, usb, bridge


def test_api_default_read_only_and_rejects_wrong_origin_host(client):
    c, usb, _ = client
    assert c.get("/api/state").status_code == 200
    assert not usb.opened
    for headers in [{}, {"origin": "https://evil.example"}, {**ORIGIN, "host": "evil.example"}]:
        assert c.post("/api/boards/D6/connect", headers=headers).status_code == 403
    assert c.get("/api/state", headers={"origin": "https://evil.example"}).status_code == 403
    assert c.post("/api/boards/STM32/connect", headers=ORIGIN).status_code == 404
    assert c.post("/api/boards/D6/command", headers=ORIGIN, json={"command": "factory-reset"}).status_code == 422
    assert not usb.opened


def test_ws_handshake_initial_snapshot_connection_and_low_power_gate(client):
    c, usb, bridge = client
    with pytest.raises(WebSocketDisconnect):
        with c.websocket_connect("ws://127.0.0.1:8787/api/stream", headers={"origin": "https://evil.example"}):
            pass
    with c.websocket_connect("ws://127.0.0.1:8787/api/stream", headers=ORIGIN) as ws:
        message = ws.receive_json()
        assert message["type"] == "snapshot" and message["state"]["mode"] == "test"
        assert c.post("/api/boards/D6/connect", headers=ORIGIN).status_code == 200
        assert len(usb.opened) == 1
        for payload in [{"command": "on\noff"}, {"command": "status", "path": "/dev/anything"}, {"command": "tx-low"}]:
            assert c.post("/api/boards/D6/command", headers=ORIGIN, json=payload).status_code == 422
        assert c.post("/api/boards/D6/disconnect", headers=ORIGIN).status_code == 200
        assert usb.opened[0].closed
        assert c.post("/api/boards/D6/command", headers=ORIGIN, json={"command": "status"}).status_code == 409
    assert not bridge.subscribers


def test_archives_download_raw_and_decoded_utf8_without_arbitrary_files(client):
    c, usb, bridge = client
    text = "I (10) LAYER_8_UART: MESH_RX source=0x0003 id=0x21 result=queued"
    c.portal.call(bridge.log, "D6", text)
    for _ in range(100):
        state = c.get("/api/state").json()["storage"]
        if state["saved"]:
            break
        time.sleep(0.01)
    assert state["saved"] == 1 and state["error"] is None
    listing = c.get("/api/logs").json()
    name = listing["files"][0]["name"]
    response = c.get(f"/api/logs/{name}")
    assert response.status_code == 200
    row = json.loads(response.content)
    assert row["text"] == text and row["event"]["label"] == "후방 경고"
    assert "후방 경고".encode() in response.content
    assert int(response.headers["content-length"]) == len(response.content)
    assert "attachment" in response.headers["content-disposition"]
    assert not usb.opened
    directory = bridge.archive.directory
    (directory / "secret.txt").write_text("not downloadable")
    (directory / "link.jsonl").symlink_to(directory / "secret.txt")
    assert c.get("/api/logs/secret.txt").status_code == 404
    assert c.get("/api/logs/link.jsonl").status_code == 404
    assert c.get("/api/logs/%2e%2e%2fsecret.txt").status_code == 404
    assert c.get("/api/logs", headers={"origin": "https://evil.example"}).status_code == 403
    assert len(c.get("/api/logs").json()["files"]) == 1
