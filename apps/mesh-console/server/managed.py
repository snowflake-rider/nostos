"""Opt-in, loopback-only server for TUI queries. Manual server behavior is unchanged."""
import argparse
import asyncio
from contextlib import suppress
import errno
import socket
import time

import uvicorn

from .app import create_app


class Activity:
    """Keep active WebSockets/HTTP transfers alive, including other UI clients."""
    def __init__(self, app):
        self.app = app
        self.active = 0
        self.last_used = time.monotonic()

    def touch(self):
        self.last_used = time.monotonic()

    async def __call__(self, scope, receive, send):
        if scope["type"] not in ("http", "websocket"):
            return await self.app(scope, receive, send)
        self.active += 1
        self.touch()
        try:
            return await self.app(scope, receive, send)
        finally:
            self.active -= 1
            self.touch()


async def expire_idle(server, activity, idle_seconds):
    while not server.started and not server.should_exit:
        await asyncio.sleep(.1)
    activity.touch()
    while not server.should_exit:
        if activity.active:
            activity.touch()
        elif time.monotonic() - activity.last_used >= idle_seconds:
            server.should_exit = True
            return
        await asyncio.sleep(.1)


async def serve(listener, app=None, idle_seconds=10):
    activity = Activity(app if app is not None else create_app())
    config = uvicorn.Config(activity, host="127.0.0.1", port=listener.getsockname()[1],
                            log_level="error", access_log=False, timeout_graceful_shutdown=3)
    server = uvicorn.Server(config)
    watchdog = asyncio.create_task(expire_idle(server, activity, idle_seconds))
    try:
        await server.serve(sockets=[listener])
    finally:
        watchdog.cancel()
        with suppress(asyncio.CancelledError):
            await watchdog


def main(argv=None, *, app=None, idle_seconds=10):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8787)
    args = parser.parse_args(argv)
    if not 1 <= args.port <= 65535:
        parser.error("port must be 1..65535")
    # Bind before starting the app: the kernel arbitrates concurrent starters.
    # SO_REUSEADDR permits restart after TIME_WAIT, not multiple live listeners.
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            listener.bind(("127.0.0.1", args.port))
            listener.listen(128)
        except OSError as exc:
            if exc.errno == errno.EADDRINUSE:
                return 75
            raise
        asyncio.run(serve(listener, app=app, idle_seconds=idle_seconds))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
