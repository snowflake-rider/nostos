"""Prepare a shared local Console; never install packages or stop another process."""
import errno
import http.client
import json
from pathlib import Path
import re
import subprocess
import time

APP = Path(__file__).resolve().parents[1] / "apps/mesh-console"
IDENTITIES = {"D6": "14:C1:9F:CE:F0:D4", "76": "14:C1:9F:CE:EC:74", "B6": "44:1B:F6:FF:BA:B4"}


def probe_console(port):
    """Return identity, or None ONLY for refused connections. No redirects/proxies."""
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=.75)
    try:
        connection.request("GET", "/api/state")
        response = connection.getresponse()
        if response.status != 200:
            raise RuntimeError("CONSOLE_PORT_NOT_COMPATIBLE")
        body = response.read(128 * 1024 + 1)
        if len(body) > 128 * 1024:
            raise RuntimeError("CONSOLE_PORT_NOT_COMPATIBLE")
        state = json.loads(body)
        if not isinstance(state, dict) or state.get("type") != "state":
            raise RuntimeError("CONSOLE_PORT_NOT_COMPATIBLE")
        if state.get("mode") != "live":
            raise RuntimeError("CONSOLE_NOT_LIVE")
        nodes = state.get("nodes")
        if (not re.fullmatch(r"[a-f0-9]{32}", str(state.get("instance", "")))
                or not isinstance(nodes, list) or len(nodes) != 3
                or any(not isinstance(n, dict) for n in nodes)
                or {n.get("board"): n.get("serial") for n in nodes} != IDENTITIES):
            raise RuntimeError("CONSOLE_PORT_NOT_COMPATIBLE")
        return state["instance"]
    except ConnectionRefusedError:
        return None
    except OSError as exc:
        if exc.errno == errno.ECONNREFUSED:
            return None
        raise RuntimeError("CONSOLE_PROBE_FAILED") from exc
    except (ValueError, TypeError, http.client.HTTPException) as exc:
        raise RuntimeError("CONSOLE_PORT_NOT_COMPATIBLE") from exc
    finally:
        connection.close()


def launch_console(port):
    python = APP / ".venv/bin/python"
    if not python.is_file():
        raise RuntimeError("CONSOLE_ENVIRONMENT_MISSING")
    try:
        # The server self-expires after idle time. Detaching protects other clients
        # when this query is cancelled; no PID files, process-group kills or services.
        return subprocess.Popen([str(python), "-B", "-m", "server.managed", "--port", str(port)],
                                cwd=APP, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL, start_new_session=True)
    except OSError as exc:
        raise RuntimeError("CONSOLE_START_FAILED") from exc


def ensure_console(port, timeout=8, probe=None, launch=None):
    probe, launch = probe or probe_console, launch or launch_console
    instance = probe(port)
    if instance:
        return instance
    child = launch(port)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        instance = probe(port)
        if instance:
            return instance
        # 75 means another simultaneous starter owns the bind; await that winner.
        if child.poll() not in (None, 75):
            raise RuntimeError("CONSOLE_START_FAILED")
        time.sleep(.1)
    raise RuntimeError("CONSOLE_START_TIMEOUT")
