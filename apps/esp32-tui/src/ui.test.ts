import { afterEach, beforeEach, expect, setSystemTime, test } from "bun:test";
import { createTestRenderer } from "@opentui/core/testing";
import { createDashboard } from "./ui";
import { ageLabel, demoReport, parseReport, type Report } from "./report";
import { readBounded, scannerCommand } from "./scanner";

let setup: Awaited<ReturnType<typeof createTestRenderer>> | null = null;
beforeEach(() => setSystemTime(new Date("2026-08-28T15:00:00.000Z")));
afterEach(() => { setup?.renderer.destroy(); setup = null; setSystemTime(); });
const task = (report = demoReport()) => ({ promise: Promise.resolve(report), cancel() {} });
async function dashboard(width = 100, height = 30, demo = true) {
  setup = await createTestRenderer({ width, height });
  const app = createDashboard(setup.renderer, () => task(), demo);
  await app.refresh(); await setup.renderOnce();
  return { app, frame: () => setup!.captureCharFrame(), key: async (name: string) => {
    setup!.mockInput.pressKey(name === "pagedown" ? "\x1b[6~" : name);
    await setup!.renderOnce();
  } };
}

test("demo shows all boards, model fields, and permanent demo label", async () => {
  const ui = await dashboard();
  expect(ui.frame()).toContain("DEMO");
  for (const board of ["D6", "76", "B6"]) expect(ui.frame()).toContain(board);
  expect(ui.frame()).toContain("C001");
  expect(ui.frame()).toContain("C000");
  expect(ui.frame()).toContain("미준비 보고 (0)");
  expect(ui.frame()).toContain("q / Esc");
  expect(ui.frame()).toMatchSnapshot();
});

test("number keys select board; help toggle preserves read-only scope", async () => {
  const ui = await dashboard();
  await ui.key("2"); expect(ui.app.state.selected).toBe(1);
  expect(ui.frame()).toContain("76 · 설정 요약");
  await ui.key("h"); expect(ui.frame()).toContain("Generic OnOff Client");
  expect(ui.frame()).toContain("화면에서 변경하지 않음");
  await ui.key("h"); expect(ui.frame()).toContain("76 · 설정 요약");
  await ui.key("3"); expect(ui.app.state.selected).toBe(2);
});

test("page scrolling reaches unavailable fields", async () => {
  const ui = await dashboard(80, 24);
  await ui.key("pagedown"); await ui.key("pagedown");
  expect(ui.frame()).toContain("읽을 수 없는 항목");
  expect(ui.frame()).toContain("현재 Relay");
});

test("small terminal hides sidebar but keeps board keys and exit usable", async () => {
  const ui = await dashboard(48, 18);
  expect(ui.frame()).toContain("1:D6");
  expect(ui.frame()).toContain("2:76");
  expect(ui.frame()).toContain("q 종료");
  expect(ui.frame()).toMatchSnapshot();
  setup!.resize(100, 30); await setup!.renderOnce();
  expect(ui.frame()).toContain("같은 조회 엔진");
});

test("refresh failure clearly labels preserved values as previous observations", async () => {
  setup = await createTestRenderer({ width: 100, height: 30 });
  let calls = 0;
  const app = createDashboard(setup.renderer, () => ++calls === 1 ? task() :
    { promise: Promise.reject(new Error("private diagnostic content")), cancel() {} });
  await app.refresh(); await app.refresh(); await setup.renderOnce();
  const frame = setup.captureCharFrame();
  expect(frame).toContain("조회 실패");
  expect(frame).toContain("이전 관찰");
  expect(frame).not.toContain("private diagnostic");
});

test("partial reports never show absent values as zero or successful read", async () => {
  setup = await createTestRenderer({ width: 100, height: 30 });
  const report = demoReport(); report.result = "INCOMPLETE";
  report.devices[0]!.status = null; report.devices[0]!.result = "CONSOLE_NOT_CONNECTED";
  const app = createDashboard(setup.renderer, () => task(report));
  await app.refresh(); await setup.renderOnce();
  expect(setup.captureCharFrame()).toContain("일부 보드를 읽지 못했습니다");
  expect(setup.captureCharFrame()).toContain("CONSOLE_NOT_CONNECTED");
  expect(setup.captureCharFrame()).toContain("송신 Client: 조회 불가");
});

test("rapid refresh is single-flight and quit cancels only owned query", async () => {
  setup = await createTestRenderer({ width: 80, height: 24 });
  let calls = 0, cancelled = 0;
  let resolve!: (r: Report) => void;
  const promise = new Promise<Report>(r => { resolve = r; });
  const app = createDashboard(setup.renderer, () => { calls++; return { promise, cancel() { cancelled++; } }; });
  const first = app.refresh();
  await app.refresh(); await app.refresh();
  expect(calls).toBe(1);
  setup.mockInput.pressKey("q");
  expect(cancelled).toBe(1); expect(app.state.disposed).toBe(true);
  resolve(demoReport()); await first;
  expect(app.state.report).toBeNull();
});

test("age label does not imply an active USB connection", () => {
  const report = demoReport(); report.checkedAt = "2026-08-28T00:00:00Z";
  expect(ageLabel(report, Date.parse(report.checkedAt) + 16000)).toBe("지난 관찰 · 16초 전");
  expect(ageLabel(null)).toContain("아직 조회하지 않음");
});

test("scanner argv has no control commands and does not shell interpolate", () => {
  const command = scannerCommand({ port: 8787, noConnect: true });
  expect(command.slice(2)).toEqual(["--json", "--ensure-console", "--port", "8787", "--no-connect"]);
  expect(command.join(" ")).not.toContain("--send");
});

test("bounded pipe reader handles UTF-8 fragments and rejects oversized output", async () => {
  const bytes = new TextEncoder().encode("한글");
  const stream = new ReadableStream<Uint8Array>({ start(c) { c.enqueue(bytes.slice(0, 1)); c.enqueue(bytes.slice(1)); c.close(); } });
  expect(await readBounded(stream)).toBe("한글");
  const huge = new ReadableStream<Uint8Array>({ start(c) { c.enqueue(new Uint8Array(10)); c.close(); } });
  await expect(readBounded(huge, 5)).rejects.toThrow("OUTPUT_TOO_LARGE");
});

test("parser rejects unexpected reports and drops non-display fields", () => {
  for (const raw of ["{}", "not json", JSON.stringify({ schema: 1, devices: [] })]) {
    expect(() => parseReport(raw)).toThrow();
  }
  // Build from the scanner's actual wire shape, not the UI's normalized object.
  const normalized = demoReport();
  const wire = { schema: 1, backend: "mesh-console", result: "STATUS_READ_PARTIAL_CONFIG",
    checked_at: normalized.checkedAt, mesh_test_transmissions_sent: 0, secret: "DO_NOT_RENDER",
    devices: normalized.devices.map(d => ({ board: d.board, usb_path: d.path, usb_serial: d.serial,
      status_result: d.result, status_received_at: d.receivedAt, firmware_report: { ...d.status, key: "DO_NOT_RENDER" } })) };
  expect(JSON.stringify(parseReport(JSON.stringify(wire)))).not.toContain("DO_NOT_RENDER");
  wire.devices[0]!.usb_path = "\x1b]52;unsafe";
  expect(() => parseReport(JSON.stringify(wire))).toThrow("INVALID_REPORT");
});
