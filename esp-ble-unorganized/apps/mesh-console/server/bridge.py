"""One event-loop owns serial handles; no reset, retries or automatic reconnect."""
import asyncio
from collections import deque
from contextlib import suppress
from dataclasses import dataclass, field
from datetime import datetime, timezone
import time
import uuid

import serial
from serial.tools import list_ports

from .protocol import LineDecoder, parse_line

IDENTITIES = {"D6": "14:C1:9F:CE:F0:D4", "B6": "44:1B:F6:FF:BA:B4", "76": "14:C1:9F:CE:EC:74"}
NAMES = {"D6": "ESP32-L8-F0D6", "B6": "ESP32-L8-BAB6", "76": "ESP32-L8-EC76"}
COMMANDS = frozenset(("status", "on", "off", "on-unack", "off-unack", "tx-low", "tx-normal"))
LOG_LIMIT = 5000
LOG_BYTES_LIMIT = 5 * 1024 * 1024
FRESH_SECONDS = 15


class BridgeError(Exception):
    def __init__(self, message, status=409):
        super().__init__(message)
        self.status = status


class NoControlSerial(serial.Serial):
    # Same no-DTR/RTS policy as Layer 8 tools/check_uart_diag.py.
    # OS/USB-driver behavior is not a universal guarantee of reset-free hardware.
    def _update_dtr_state(self):
        pass

    def _update_rts_state(self):
        pass


def open_serial(path):
    return NoControlSerial(path, 115200, timeout=0, write_timeout=0.3,
                           exclusive=True, xonxoff=False, rtscts=False, dsrdtr=False)


@dataclass
class Node:
    board: str
    path: str | None = None
    port: object | None = None
    phase: str = "disconnected"
    error: str | None = None
    status: dict | None = None
    status_at: float | None = None
    status_mono: float = 0
    poll_at: float = 0
    command_at: float = -100
    last_power_request: str | None = None
    decoder: LineDecoder = field(default_factory=LineDecoder)
    lock: asyncio.Lock = field(default_factory=asyncio.Lock)


class Bridge:
    def __init__(self, scanner=list_ports.comports, opener=open_serial, mode="live"):
        self.scanner, self.opener, self.mode = scanner, opener, mode
        self.nodes = {board: Node(board) for board in IDENTITIES}
        self.logs = deque()
        self.log_bytes = 0
        self.seq = 0
        self.dropped = 0
        self.instance = uuid.uuid4().hex
        self.subscribers = set()
        self.task = None
        self.empty_since = None
        self.closed = False
        self.scan_error = None

    def get_node(self, board):
        if board not in self.nodes:
            raise BridgeError("알 수 없는 보드입니다.", 404)
        return self.nodes[board]

    def refresh(self):
        try:
            ports = list(self.scanner())
            self.scan_error = None
        except (OSError, serial.SerialException) as exc:
            self.scan_error = f"USB 목록을 읽지 못했습니다: {exc}"
            return
        for board, node in self.nodes.items():
            matches = [p.device for p in ports if (p.serial_number or "").upper() == IDENTITIES[board]
                       and p.vid == 0x303A and p.pid == 0x1001]
            node.path = matches[0] if len(matches) == 1 else None

    def snapshot(self):
        now = time.monotonic()
        nodes = []
        for board, n in self.nodes.items():
            fresh = n.port is not None and n.status is not None and now - n.status_mono < FRESH_SECONDS
            nodes.append({"board": board, "serial": IDENTITIES[board], "name": NAMES[board],
                          "detected": n.path is not None, "path": n.path, "phase": n.phase,
                          "error": n.error, "status": n.status if n.port else None,
                          "status_at": n.status_at, "fresh": fresh,
                          "last_power_request": n.last_power_request})
        return {"type": "state", "instance": self.instance, "mode": self.mode, "nodes": nodes,
                "seq": self.seq, "dropped": self.dropped, "scan_error": self.scan_error,
                "poll_seconds": 5, "limit": LOG_LIMIT}

    def publish(self, event):
        for queue in tuple(self.subscribers):
            if queue.full():
                # Tell the client to reconnect for a bounded snapshot; never grow queues.
                while not queue.empty():
                    queue.get_nowait()
                queue.put_nowait({"type": "overflow"})
                self.subscribers.discard(queue)
            else:
                queue.put_nowait(event)

    def log(self, board, text, direction="rx", level=None):
        parsed = parse_line(text)
        n = self.nodes.get(board)
        if n and direction == "rx":
            if "[LAYER-8] BOOT_START" in text:
                n.status = None
                n.status_at = None
                n.phase = "verifying"
            if parsed["status"] and parsed["status"]["name"] == NAMES[board]:
                n.status = parsed["status"]
                n.status_at = time.time() * 1000
                n.status_mono = time.monotonic()
                n.phase = "connected"
        self.seq += 1
        row = {"id": self.seq, "time": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
               "board": board, "direction": direction, "text": text,
               "category": "COMMAND" if direction == "tx" else "SYSTEM" if direction == "system" else parsed["category"],
               "level": level or parsed["level"], "uptime_ms": parsed["uptime_ms"]}
        self.logs.append(row)
        self.log_bytes += len(text.encode("utf-8"))
        while len(self.logs) > LOG_LIMIT or self.log_bytes > LOG_BYTES_LIMIT:
            old = self.logs.popleft()
            self.log_bytes -= len(old["text"].encode("utf-8"))
            self.dropped += 1
        self.publish({"type": "log", "log": row})

    async def connect(self, board):
        n = self.get_node(board)
        async with n.lock:
            if self.closed:
                raise BridgeError("서버가 종료 중입니다.")
            if not self.subscribers:
                raise BridgeError("먼저 웹 화면의 실시간 연결을 확인해 주세요.")
            if n.port:
                return
            self.refresh()
            if self.scan_error:
                raise BridgeError(self.scan_error)
            if not n.path:
                raise BridgeError("해당 USB serial의 보드가 없거나 중복 감지됐습니다.")
            n.phase, n.error, n.status, n.status_at = "connecting", None, None, None
            self.publish(self.snapshot())
            try:
                n.port = self.opener(n.path)
            except (OSError, serial.SerialException) as exc:
                n.phase = "error"
                n.error = f"포트를 열지 못했습니다. 다른 모니터를 닫고 다시 시도하세요. ({exc})"
                self.log(board, n.error, "system", "error")
                self.publish(self.snapshot())
                raise BridgeError(n.error) from exc
            n.decoder = LineDecoder()
            n.phase = "verifying"
            n.last_power_request = None
            n.command_at = -100
            self.log(board, "USB 연결됨 · 115200 / 8N1 · 상태 조회 5초 간격", "system")
        await self.command(board, "status", automatic=True)
        self.publish(self.snapshot())

    def _close(self, n, error=None):
        port, n.port = n.port, None
        if port:
            with suppress(OSError, serial.SerialException):
                port.close()
        n.phase = "error" if error else "disconnected"
        n.error = error
        n.status, n.status_at, n.last_power_request = None, None, None
        n.decoder = LineDecoder()

    async def disconnect(self, board, reason="USB 연결 해제됨"):
        n = self.get_node(board)
        async with n.lock:
            had_port = n.port is not None
            self._close(n)
            if had_port:
                self.log(board, reason, "system")
            self.publish(self.snapshot())

    async def command(self, board, command, automatic=False):
        n = self.get_node(board)
        if command not in COMMANDS:
            raise BridgeError("허용되지 않은 명령입니다.", 422)
        async with n.lock:
            if not n.port:
                raise BridgeError("보드를 먼저 연결해 주세요.")
            now = time.monotonic()
            fresh = n.status is not None and now - n.status_mono < FRESH_SECONDS
            if command != "status" and not fresh:
                raise BridgeError("최신 Layer 8 상태를 먼저 확인해 주세요.")
            if command.startswith(("on", "off")) and n.status.get("onoff_ready") != "1":
                raise BridgeError("C000 On/Off 모델이 준비되지 않았습니다. nRF Mesh 설정을 확인하세요.")
            if not automatic and now - n.command_at < 0.3:
                raise BridgeError("명령을 너무 빠르게 보냈습니다. 잠시 후 다시 시도하세요.", 429)
            data = (command + "\n").encode("ascii")
            try:
                written = n.port.write(data)
                if written != len(data):
                    raise serial.SerialTimeoutException("부분 전송 — 자동 재시도하지 않음")
            except (OSError, serial.SerialException) as exc:
                message = f"USB 전송 실패: {exc}. 자동 재시도하지 않습니다."
                self._close(n, message)
                self.log(board, message, "system", "error")
                self.publish(self.snapshot())
                raise BridgeError(message) from exc
            n.poll_at = now if command == "status" else n.poll_at
            if not automatic:
                n.command_at = now
                self.log(board, f"{command} · USB에 전달됨 (실행·상대 수신 확인 아님)", "tx")
            if command.startswith("tx-"):
                n.last_power_request = command
            self.publish(self.snapshot())
            return {"result": "written", "command": command, "board": board}

    async def start(self):
        self.refresh()
        self.task = asyncio.create_task(self._run())

    async def _run(self):
        last_scan = last_state = 0
        while not self.closed:
            now = time.monotonic()
            if now - last_scan > 2:
                self.refresh()
                last_scan = now
            if self.subscribers:
                self.empty_since = None
            elif any(n.port for n in self.nodes.values()):
                self.empty_since = self.empty_since or now
                if now - self.empty_since > 2:
                    for board in self.nodes:
                        await self.disconnect(board, "모든 웹 화면이 닫혀 USB 포트를 해제했습니다.")
            for board, n in self.nodes.items():
                if not n.port:
                    continue
                try:
                    if n.path != n.port.port:
                        raise serial.SerialException("USB가 분리되었거나 포트가 변경됐습니다")
                    data = n.port.read(min(max(n.port.in_waiting, 1), 8192))
                    for line in n.decoder.feed(data):
                        self.log(board, line)
                except (OSError, serial.SerialException) as exc:
                    self._close(n, str(exc))
                    self.log(board, f"USB 연결 끊김: {exc}", "system", "error")
                    self.publish(self.snapshot())
                    continue
                if now - n.poll_at >= 5:
                    with suppress(BridgeError):
                        await self.command(board, "status", automatic=True)
            if now - last_state > 1:
                self.publish(self.snapshot())
                last_state = now
            await asyncio.sleep(0.05)

    async def stop(self):
        self.closed = True
        if self.task:
            self.task.cancel()
            with suppress(asyncio.CancelledError):
                await self.task
        for n in self.nodes.values():
            self._close(n)
