import { expect, test } from "bun:test";
import { watchScan } from "./scanner";
import { demoReport } from "./report";

function spawn(code: string) {
  return Bun.spawn([process.execPath, "-e", code], { stdin: "ignore", stdout: "pipe", stderr: "pipe" });
}

test("scanner accepts validated partial JSON even with exit code 2", async () => {
  const demo = demoReport();
  const wire = { schema: 1, backend: "mesh-console", mesh_test_transmissions_sent: 0,
    checked_at: demo.checkedAt, result: "INCOMPLETE", devices: demo.devices.map(d => ({
      board: d.board, usb_path: d.path, usb_serial: d.serial, status_result: "STATUS_TIMEOUT",
      status_received_at: null, firmware_report: null,
    })) };
  const child = spawn(`console.log(${JSON.stringify(JSON.stringify(wire))}); process.exitCode = 2;`);
  const report = await watchScan(child).promise;
  expect(report.result).toBe("INCOMPLETE");
  expect(report.devices.every(d => d.status === null)).toBe(true);
  expect(child.exitCode).toBe(2);
});

test("scanner rejects process failures and malformed output", async () => {
  await expect(watchScan(spawn("process.exitCode = 3;")).promise).rejects.toThrow("SCAN_PROCESS_FAILED");
  await expect(watchScan(spawn('console.log("not JSON")')).promise).rejects.toThrow();
});

test("cancelling a running query waits until its own process exits", async () => {
  const child = spawn("setInterval(() => {}, 1000);");
  const scan = watchScan(child);
  scan.cancel();
  await expect(scan.promise).rejects.toThrow("SCAN_CANCELLED_OR_TIMED_OUT");
  expect(child.signalCode).toBe("SIGINT");
  scan.cancel(); // Repeated cleanup is harmless.
});

test("query timeout terminates the process and rejects instead of hanging", async () => {
  const child = spawn("setInterval(() => {}, 1000);");
  await expect(watchScan(child, 50).promise).rejects.toThrow("SCAN_CANCELLED_OR_TIMED_OUT");
  expect(child.signalCode).toBe("SIGINT");
});
