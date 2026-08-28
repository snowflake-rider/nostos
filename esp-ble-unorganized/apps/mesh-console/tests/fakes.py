import time
from types import SimpleNamespace

from server.bridge import IDENTITIES, NAMES


def status_line(board="D6", ready=1):
    return (f"I (4020) LAYER_8_MESH: STATUS name={NAMES[board]} primary=0x0005 "
            f"event_ready=1 net=0x0000 app=0x0001 pub=0xc001 sub_C001=1 ttl=7 "
            f"period=0 retransmit=0 relay=0 onoff_ready={ready} state=0")


class FakePort:
    def __init__(self, path, live=False):
        self.port = path
        self.board = path.rsplit("/", 1)[-1]
        self.closed = False
        self.writes = []
        self.buffer = bytearray()
        self.live = live
        self.last_event = 0
        self.sequence = 0
        self.partial_write = False
        self.silent = False

    @property
    def in_waiting(self):
        return len(self.buffer)

    def write(self, data):
        assert not self.closed
        self.writes.append(data)
        if self.partial_write:
            return 1
        if not self.silent:
            if data == b"status\n":
                self.feed(status_line(self.board, 0 if self.board == "B6" else 1))
            else:
                self.feed(f"I (5100) LAYER_8_CONSOLE: TEST command={data.decode().strip()} api=accepted peer_ACK=none")
        return len(data)

    def feed(self, line):
        self.buffer.extend((line + "\r\n").encode())

    def read(self, size):
        if self.live and time.monotonic() - self.last_event > 0.8:
            self.last_event = time.monotonic()
            self.sequence += 1
            markers = [f"UART_RX id=0x13 result=queued test_seq={self.sequence}",
                       f"MESH_TX id=0x13 source=0x0005 api=accepted test_seq={self.sequence}",
                       f"MESH_RX source=0x0006 id=0x13 result=queued test_seq={self.sequence}",
                       f"UART_TX id=0x13 source=0x0006 api=accepted test_seq={self.sequence}",
                       f"ERROR TEST_ONLY simulated_failure seq={self.sequence}"]
            self.feed(f"I ({self.sequence * 800}) LAYER_8_TEST: {markers[self.sequence % len(markers)]}")
        data = self.buffer[:size]
        del self.buffer[:size]
        return bytes(data)

    def close(self):
        self.closed = True


class FakeUSB:
    def __init__(self, live=False):
        self.live = live
        self.present = list(IDENTITIES)
        self.opened = []

    def scan(self):
        return [SimpleNamespace(serial_number=IDENTITIES[board], device=f"/fake/{board}",
                                vid=0x303A, pid=0x1001) for board in self.present]

    def open(self, path):
        port = FakePort(path, self.live)
        self.opened.append(port)
        return port
