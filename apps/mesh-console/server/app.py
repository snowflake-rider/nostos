"""Same-origin local UI/API; mutations require a live UI and an allowed Origin."""
import asyncio
import os
import re
import stat
from contextlib import asynccontextmanager, suppress
from pathlib import Path
from urllib.parse import urlsplit

from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, ConfigDict, Field
from starlette.background import BackgroundTask

from .bridge import Bridge, BridgeError


class CommandBody(BaseModel):
    model_config = ConfigDict(extra="forbid")
    command: str = Field(max_length=16)
    confirm_low_power: bool = False


class LocalBoundary:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] not in ("http", "websocket"):
            return await self.app(scope, receive, send)
        headers = dict(scope.get("headers", []))
        host = headers.get(b"host", b"").decode("latin-1")
        try:
            hostname = urlsplit("//" + host).hostname
        except ValueError:
            hostname = None
        origin = headers.get(b"origin", b"").decode("latin-1")
        port = scope.get("server", (None, 8787))[1]
        allowed = {f"http://127.0.0.1:{port}", f"http://localhost:{port}",
                   "http://127.0.0.1:5173", "http://localhost:5173"}
        mutation = scope["type"] == "websocket" or scope.get("method") not in ("GET", "HEAD")
        bad = hostname not in ("localhost", "127.0.0.1") or (origin and origin not in allowed) or (mutation and not origin)
        if bad:
            if scope["type"] == "websocket":
                return await send({"type": "websocket.close", "code": 1008})
            return await JSONResponse({"detail": "로컬 앱에서만 접근할 수 있습니다."}, 403)(scope, receive, send)
        return await self.app(scope, receive, send)


def create_app(bridge=None):
    bridge = bridge or Bridge()

    @asynccontextmanager
    async def lifespan(app):
        await bridge.start()
        try:
            yield
        finally:
            await bridge.stop()

    app = FastAPI(lifespan=lifespan, docs_url=None, redoc_url=None, openapi_url=None)
    app.add_middleware(LocalBoundary)
    app.state.bridge = bridge

    @app.exception_handler(BridgeError)
    async def bridge_error(request, error):
        return JSONResponse({"detail": str(error)}, error.status)

    @app.middleware("http")
    async def security_headers(request: Request, call_next):
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["Referrer-Policy"] = "no-referrer"
        response.headers["Cache-Control"] = "no-store" if request.url.path.startswith("/api") else "no-cache"
        response.headers["Content-Security-Policy"] = "default-src 'self'; connect-src 'self' ws://127.0.0.1:* ws://localhost:*; style-src 'self' 'unsafe-inline'; font-src 'self'; img-src 'self' data:; object-src 'none'; frame-ancestors 'none'; base-uri 'self'"
        return response

    @app.get("/api/state")
    async def state():
        bridge.refresh()
        return bridge.snapshot()

    def archive_directory():
        if bridge.archive.directory is None:
            raise HTTPException(404, "파일 저장을 사용하지 않는 서버입니다.")
        return bridge.archive.directory

    @app.get("/api/logs")
    def archives():
        directory = archive_directory()
        try:
            files = sorted((p for p in directory.glob("*.jsonl")
                            if re.fullmatch(r"[A-Za-z0-9_-]+\.jsonl", p.name)
                            and not p.is_symlink() and p.is_file()),
                           key=lambda p: p.stat().st_mtime, reverse=True)
            return {"files": [{"name": p.name,
                               "bytes": bridge.archive.committed_bytes if p == bridge.archive.path else p.stat().st_size,
                               "active": p == bridge.archive.path} for p in files[:100]],
                    "total": len(files), "directory": str(directory)}
        except OSError as exc:
            raise HTTPException(503, "저장 파일 목록을 읽지 못했습니다.") from exc

    @app.get("/api/logs/{name}")
    def download_archive(name: str):
        # No user-supplied directories, symlinks, or arbitrary file access.
        if not re.fullmatch(r"[A-Za-z0-9_-]+\.jsonl", name):
            raise HTTPException(404, "로그 파일을 찾을 수 없습니다.")
        path = archive_directory() / name
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        except OSError as exc:
            raise HTTPException(404, "로그 파일을 찾을 수 없습니다.") from exc
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode):
            os.close(fd)
            raise HTTPException(404, "로그 파일을 찾을 수 없습니다.")
        length = min(info.st_size, bridge.archive.committed_bytes) if path == bridge.archive.path else info.st_size
        source = os.fdopen(fd, "rb")

        def chunks():
            with source:
                remaining = length
                while remaining:
                    chunk = source.read(min(65536, remaining))
                    if not chunk:
                        break
                    remaining -= len(chunk)
                    yield chunk

        return StreamingResponse(chunks(), media_type="application/x-ndjson",
                                 headers={"Content-Disposition": f'attachment; filename="{name}"',
                                          "Content-Length": str(length)},
                                 background=BackgroundTask(source.close))

    @app.post("/api/boards/{board}/connect")
    async def connect(board: str):
        await bridge.connect(board)
        return bridge.snapshot()

    @app.post("/api/boards/{board}/disconnect")
    async def disconnect(board: str):
        await bridge.disconnect(board)
        return bridge.snapshot()

    @app.post("/api/boards/{board}/command")
    async def command(board: str, body: CommandBody):
        if body.command == "tx-low" and not body.confirm_low_power:
            raise BridgeError("낮은 송신 출력으로 변경하려면 확인이 필요합니다.", 422)
        return await bridge.command(board, body.command)

    @app.websocket("/api/stream")
    async def stream(socket: WebSocket):
        await socket.accept()
        queue = asyncio.Queue(maxsize=256)
        bridge.subscribers.add(queue)

        async def receive_until_close():
            while True:
                event = await socket.receive()
                if event["type"] == "websocket.disconnect":
                    return

        async def send_events():
            await socket.send_json({"type": "snapshot", "state": bridge.snapshot(), "logs": list(bridge.logs)})
            while True:
                event = await queue.get()
                if event["type"] == "overflow":
                    await socket.close(code=1013, reason="로그 수신 지연: 다시 연결해 주세요")
                    return
                await socket.send_json(event)

        tasks = [asyncio.create_task(receive_until_close()), asyncio.create_task(send_events())]
        try:
            await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
        except WebSocketDisconnect:
            pass
        finally:
            bridge.subscribers.discard(queue)
            for task in tasks:
                task.cancel()
            for task in tasks:
                with suppress(asyncio.CancelledError, WebSocketDisconnect, RuntimeError):
                    await task

    dist = Path(__file__).resolve().parents[1] / "dist"
    if dist.is_dir():
        app.mount("/assets", StaticFiles(directory=dist / "assets"), name="assets")

    @app.get("/")
    async def index():
        if (dist / "index.html").exists():
            return FileResponse(dist / "index.html")
        return JSONResponse({"detail": "먼저 npm run build를 실행해 주세요."}, 503)

    @app.get("/favicon.svg")
    async def favicon():
        return FileResponse(Path(__file__).resolve().parents[1] / "public/favicon.svg")

    return app


app = create_app()
