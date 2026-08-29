export const BOARDS = ["D6", "76", "B6"] as const;
export type Board = (typeof BOARDS)[number];
export const FIELDS = ["name", "primary", "net", "app", "pub", "sub_C001", "event_ready",
  "ttl", "period", "retransmit", "onoff_ready", "state", "relay_cached"] as const;
export type Device = {
  board: Board;
  path: string | null;
  serial: string;
  result: string;
  receivedAt: string | null;
  status: Partial<Record<(typeof FIELDS)[number], string>> | null;
};
export type Report = { checkedAt: string; result: string; devices: Device[] };

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("INVALID_REPORT");
  return value as Record<string, unknown>;
}
function text(value: unknown): string {
  if (typeof value !== "string" || value.length > 200 || /[\x00-\x1f\x7f-\x9f]/.test(value)) {
    throw new Error("INVALID_REPORT");
  }
  return value;
}
function timestamp(value: unknown): string {
  const result = text(value);
  if (!Number.isFinite(Date.parse(result))) throw new Error("INVALID_REPORT");
  return result;
}

// Rebuild only the fields displayed by the UI; arbitrary logs and key fields are discarded.
export function parseReport(raw: string): Report {
  const data = record(JSON.parse(raw));
  if (data.schema !== 1 || data.mesh_test_transmissions_sent !== 0 ||
      !["mesh-console", "usb-enumeration"].includes(String(data.backend)) ||
      !["STATUS_READ_PARTIAL_CONFIG", "INCOMPLETE", "USB_ONLY"].includes(String(data.result)) ||
      !Array.isArray(data.devices) || data.devices.length !== 3) throw new Error("INVALID_REPORT");
  const devices = BOARDS.map(board => {
    const matches = (data.devices as unknown[]).map(record).filter(d => d.board === board);
    if (matches.length !== 1) throw new Error("INVALID_REPORT");
    const d = matches[0]!;
    const result = text(d.status_result);
    if (!/^[A-Z_]+$/.test(result)) throw new Error("INVALID_REPORT");
    let status: Device["status"] = null;
    if (result === "READ") {
      const source = record(d.firmware_report);
      status = {};
      for (const field of FIELDS) status[field] = text(source[field]);
      for (const field of FIELDS.filter(f => f !== "name")) {
        if (!/^(0[xX][0-9a-fA-F]{1,4}|\d{1,5})$/.test(status[field]!)) throw new Error("INVALID_REPORT");
      }
    }
    return { board, path: d.usb_path == null ? null : text(d.usb_path), serial: text(d.usb_serial),
      result, receivedAt: d.status_received_at == null ? null : timestamp(d.status_received_at), status };
  });
  return { checkedAt: timestamp(data.checked_at), result: text(data.result), devices };
}

export function ageLabel(report: Report | null, now = Date.now()): string {
  if (!report) return "아직 조회하지 않음";
  const seconds = Math.max(0, Math.floor((now - Date.parse(report.checkedAt)) / 1000));
  return `${seconds >= 15 ? "지난 관찰" : "마지막 관찰"} · ${seconds}초 전`;
}

export function readiness(value?: string): string {
  return value === "1" ? "준비 보고 (1)" : value === "0" ? "미준비 보고 (0)" : "조회 불가";
}

export function demoReport(): Report {
  const serials = ["14:C1:9F:CE:F0:D4", "14:C1:9F:CE:EC:74", "44:1B:F6:FF:BA:B4"];
  return parseReport(JSON.stringify({ schema: 1, backend: "mesh-console", mesh_test_transmissions_sent: 0,
    result: "STATUS_READ_PARTIAL_CONFIG", checked_at: new Date().toISOString(),
    devices: BOARDS.map((board, i) => ({ board, usb_serial: serials[i], usb_path: `/dev/cu.usbmodemDEMO${i}`,
      status_result: "READ", status_received_at: new Date().toISOString(),
      firmware_report: { name: `ESP32-DEMO-${board}`, primary: ["0x0005", "0x0003", "0x0006"][i],
        net: "0x0000", app: "0x0001", pub: "0xc001", sub_C001: "1", event_ready: "1", ttl: "7",
        period: "0", retransmit: "0", onoff_ready: "0", state: "0", relay_cached: "0" } })) }));
}
