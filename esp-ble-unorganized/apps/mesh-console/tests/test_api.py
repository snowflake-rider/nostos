import pytest
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect
from server.app import create_app
from server.bridge import Bridge
from .fakes import FakeUSB

ORIGIN = {"origin": "http://127.0.0.1:8787"}


@pytest.fixture
def client():
    usb = FakeUSB()
    bridge = Bridge(usb.scan, usb.open, "test")
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
