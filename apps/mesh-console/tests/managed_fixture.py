"""Subprocess fixture: real HTTP/WS, empty USB inventory, no serial opens."""
from pathlib import Path
import sys

import uvicorn

from server.app import create_app
from server.bridge import Bridge
from server.managed import main


def no_serial(path):
    raise AssertionError("Hardware access forbidden in lifecycle test")


if __name__ == "__main__":
    port, directory, mode = int(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
    app = create_app(Bridge(scanner=lambda: [], opener=no_serial, mode="live", log_directory=directory))
    if mode == "manual":
        uvicorn.run(app, host="127.0.0.1", port=port, log_level="error", access_log=False)
    else:
        raise SystemExit(main(["--port", str(port)], app=app, idle_seconds=.8))
