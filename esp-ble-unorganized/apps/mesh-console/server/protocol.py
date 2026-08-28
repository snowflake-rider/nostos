"""Parse observed Layer 8 text, without turning API acceptance into delivery."""
import re

ANSI = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
FIELDS = re.compile(r"\b(\w+)=([^\s;]+)")
UPTIME = re.compile(r"^[IWEVD] \((\d+)\)")
STATUS_FIELDS = {"name", "primary", "event_ready", "net", "app", "pub", "sub_C001",
                 "ttl", "period", "retransmit", "relay", "onoff_ready", "state"}


class LineDecoder:
    """Bound bytes before decoding; retain fragmented UTF-8 until a full line."""
    def __init__(self, limit=8192):
        self.limit = limit
        self.pending = bytearray()
        self.discarding = False
        self.after_cr = False

    def feed(self, data):
        lines = []
        for byte in data:
            if byte in (10, 13):
                if byte == 10 and self.after_cr:
                    self.after_cr = False
                    continue
                self.after_cr = byte == 13
                if not self.discarding and self.pending:
                    text = self.pending.decode("utf-8", errors="replace")
                    text = ANSI.sub("", text)
                    text = "".join(c for c in text if c == "\t" or ord(c) >= 32)
                    if text.strip():
                        lines.append(text)
                self.pending.clear()
                self.discarding = False
            else:
                self.after_cr = False
                if self.discarding:
                    continue
                self.pending.append(byte)
                if len(self.pending) > self.limit:
                    lines.append("[BRIDGE] 너무 긴 로그 한 줄을 생략했습니다 (8 KiB 제한).")
                    self.pending.clear()
                    self.discarding = True
        return lines


def parse_line(text):
    uptime = UPTIME.match(text)
    failed = bool(re.search(r"\b(?:api=failed|result=(?:invalid|not_ready|full)|[A-Z_]*FAILED|ERROR|panic|Guru Meditation)\b", text))
    error = text.startswith("E (") or failed
    level = "error" if error else "warning" if text.startswith("W (") else "info"
    category = "OTHER"
    for marker, label in (("UART_RX id=", "UART RX"), ("MESH_TX id=", "MESH TX"),
                          ("MESH_RX source=", "MESH RX"), ("UART_TX id=", "UART TX")):
        if marker in text:
            category = label
            break
    status = None
    if "LAYER_8_MESH: STATUS " in text:
        fields = dict(FIELDS.findall(text))
        if STATUS_FIELDS <= fields.keys() and fields["name"].startswith("ESP32-L8-"):
            try:
                for key in STATUS_FIELDS - {"name"}:
                    int(fields[key], 0)
                status = {key: fields[key] for key in STATUS_FIELDS}
            except ValueError:
                pass
        category = "STATUS"
    elif category == "OTHER" and any(m in text for m in ("UART_DIAG", "QUEUE pending=", "MESH_STACK", " accepted=", " valid=")):
        category = "STATUS"
    elif category == "OTHER" and ("BOOT_START" in text or "APP_STARTED" in text or text.startswith("[BRIDGE]")):
        category = "SYSTEM"
    if error:
        category = "ERROR"
    return {"category": category, "level": level,
            "uptime_ms": int(uptime[1]) if uptime else None, "status": status}

