"""Real socket/process lifecycle regressions with no USB access."""
import concurrent.futures
import importlib.util
from pathlib import Path
import socket
import subprocess
import sys
import time
from unittest.mock import Mock

import pytest
from websockets.sync.client import connect

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
spec = importlib.util.spec_from_file_location("esp32_console", ROOT / "scripts/esp32_console.py")
console = importlib.util.module_from_spec(spec)
spec.loader.exec_module(console)


@pytest.fixture
def rig(tmp_path):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
    children = []

    def launch(selected_port, mode="auto"):
        assert selected_port == port
        child = subprocess.Popen([sys.executable, "-B", "-m", "tests.managed_fixture", str(port),
                                  str(tmp_path), mode], cwd=APP, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL)
        children.append(child)
        return child

    yield port, launch, children
    # Fixture-owned processes only; a failure never leaks the test server.
    for child in children:
        if child.poll() is None:
            child.terminate()
        child.wait(timeout=5)


def test_auto_start_reuse_then_idle_exit(rig):
    port, launch, children = rig
    first = console.ensure_console(port, launch=launch)
    assert console.ensure_console(port, launch=launch) == first
    assert len(children) == 1
    assert children[0].wait(timeout=5) == 0
    assert console.probe_console(port) is None


def test_simultaneous_starters_share_one_listener(rig):
    port, launch, children = rig
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda _: console.ensure_console(port, launch=launch), range(2)))
    assert results[0] == results[1]
    codes = sorted(child.wait(timeout=5) for child in children)
    assert codes in ([0], [0, 75])


def test_websocket_keeps_server_alive_after_other_client_leaves(rig):
    port, launch, children = rig
    instance = console.ensure_console(port, launch=launch)
    with connect(f"ws://127.0.0.1:{port}/api/stream", origin=f"http://127.0.0.1:{port}") as viewer:
        viewer.recv(timeout=2)
        with connect(f"ws://127.0.0.1:{port}/api/stream", origin=f"http://127.0.0.1:{port}") as query:
            query.recv(timeout=2)
        time.sleep(1.2)  # Exceeds auto idle timeout; the remaining UI must survive.
        assert children[0].poll() is None
        assert console.probe_console(port) == instance
    assert children[0].wait(timeout=5) == 0


def test_manual_server_is_reused_but_never_idle_stopped(rig):
    port, launch, children = rig
    console.ensure_console(port, launch=lambda p: launch(p, "manual"))
    time.sleep(1.2)
    assert console.ensure_console(port, launch=launch)
    assert len(children) == 1
    assert children[0].poll() is None


def test_http_queries_renew_idle_deadline(rig):
    port, launch, children = rig
    instance = console.ensure_console(port, launch=launch)
    for _ in range(4):
        time.sleep(.3)
        assert console.probe_console(port) == instance
    assert children[0].poll() is None
    assert children[0].wait(timeout=5) == 0


def test_cancel_during_start_leaves_server_to_self_expire(rig):
    port, launch, children = rig
    # Cancel after creating a real process but before it can report readiness.
    with pytest.raises(KeyboardInterrupt):
        console.ensure_console(port, launch=launch, probe=Mock(side_effect=[None, KeyboardInterrupt()]))
    assert children[0].wait(timeout=6) == 0
    assert console.probe_console(port) is None
