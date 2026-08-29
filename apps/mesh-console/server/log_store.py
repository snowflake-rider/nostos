"""Local UTF-8 JSONL archive; disk I/O never runs in the USB reader loop."""
import asyncio
import json
import os
from datetime import datetime, timezone
from pathlib import Path

LOG_ROOT = Path(__file__).resolve().parents[1] / "logs"
QUEUE_BYTES = 2 * 1024 * 1024


class LogStore:
    def __init__(self, directory, instance, mode):
        self.directory = Path(directory) if directory is not None else None
        self.instance, self.mode = instance, mode
        self.path = None
        self.file = None
        self.task = None
        self.queue = asyncio.Queue(maxsize=2048)
        self.pending = self.pending_bytes = self.saved = self.missed = 0
        self.committed_bytes = 0
        self.error = None
        self.closed = False

    async def start(self):
        if self.directory is None:
            return
        try:
            await asyncio.to_thread(self._open)
        except OSError as exc:
            self.error = f"로그 파일을 열지 못했습니다: {exc}"
            return
        self.task = asyncio.create_task(self._run())

    def _open(self):
        self.directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        self.path = self.directory / f"mesh-{stamp}-{self.instance}.jsonl"
        fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        self.file = os.fdopen(fd, "wb")

    def append(self, row):
        if self.directory is None:
            return
        if self.error or self.closed or not self.task:
            self.missed += 1
            return
        record = {**row, "instance": self.instance, "mode": self.mode}
        data = (json.dumps(record, ensure_ascii=False) + "\n").encode("utf-8")
        if self.queue.full() or self.pending_bytes + len(data) > QUEUE_BYTES:
            self.error = "저장 대기열 한도 초과. 일부 로그가 파일에 저장되지 않았습니다."
            self.missed += 1
            return
        self.queue.put_nowait(data)
        self.pending += 1
        self.pending_bytes += len(data)

    def _write(self, data):
        # Only completed, flushed batches count as saved or downloadable.
        position = self.file.tell()
        try:
            self.file.write(data)
            self.file.flush()
            os.fsync(self.file.fileno())
        except OSError:
            try:
                self.file.seek(position)
                self.file.truncate()
                self.file.flush()
            except OSError:
                pass
            raise

    async def _run(self):
        failed = False
        stopping = False
        while not stopping:
            first = await self.queue.get()
            if first is None:
                break
            batch = [first]
            while len(batch) < 256 and not self.queue.empty():
                item = self.queue.get_nowait()
                if item is None:
                    stopping = True
                    break
                batch.append(item)
            size = sum(map(len, batch))
            try:
                if failed:
                    self.missed += len(batch)
                else:
                    await asyncio.to_thread(self._write, b"".join(batch))
                    self.saved += len(batch)
                    self.committed_bytes += size
            except OSError as exc:
                failed = True
                self.error = f"로그 저장 실패: {exc}. 화면 수집은 계속되지만 파일 저장은 중단됐습니다."
                self.missed += len(batch)
            finally:
                self.pending -= len(batch)
                self.pending_bytes -= size

    async def stop(self):
        self.closed = True
        if self.task:
            await self.queue.put(None)
            await self.task
            self.task = None
        if self.file:
            try:
                await asyncio.to_thread(self.file.close)
            except OSError as exc:
                self.error = f"로그 파일 종료 실패: {exc}"

    def snapshot(self):
        return {"enabled": self.directory is not None,
                "directory": str(self.directory) if self.directory else None,
                "file": self.path.name if self.path else None,
                "saved": self.saved, "pending": self.pending, "missed": self.missed,
                "error": self.error}
