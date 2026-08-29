"""Explicit UI fixture: python -m uvicorn tests.preview:app --host 127.0.0.1 --port 8788.

No physical scanner or serial opener is reachable through this app.
"""
from server.app import create_app
from server.bridge import Bridge
from server.log_store import LOG_ROOT
from .fakes import FakeUSB

usb = FakeUSB(live=True)
app = create_app(Bridge(scanner=usb.scan, opener=usb.open, mode="test", log_directory=LOG_ROOT / "test"))
