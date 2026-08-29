import { resolve } from "node:path";
import { parseReport, type Report } from "./report";

export type ScanOptions = { port: number; noConnect: boolean };
export type ScanTask = { promise: Promise<Report>; cancel: () => void };
export type Scanner = () => ScanTask;
export const ROOT = resolve(import.meta.dir, "../../..");

export function scannerCommand(options: ScanOptions): string[] {
  return ["bash", resolve(ROOT, "scripts/esp32-scan"), "--json", "--ensure-console", "--port", String(options.port),
    ...(options.noConnect ? ["--no-connect"] : [])];
}

export async function readBounded(stream: ReadableStream<Uint8Array>, maximum = 128 * 1024): Promise<string> {
  const reader = stream.getReader();
  const chunks: Uint8Array[] = [];
  let length = 0;
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      length += value.length;
      if (length > maximum) { await reader.cancel(); throw new Error("OUTPUT_TOO_LARGE"); }
      chunks.push(value);
    }
  } finally { reader.releaseLock(); }
  return Buffer.concat(chunks).toString("utf8");
}

export function startScan(options: ScanOptions): ScanTask {
  const child = Bun.spawn(scannerCommand(options), { cwd: ROOT, stdin: "ignore", stdout: "pipe", stderr: "pipe" });
  return watchScan(child);
}

type ScanProcess = Bun.Subprocess<"ignore", "pipe", "pipe">;

// Separate lifecycle handling so tests can use disposable processes without USB/Console access.
export function watchScan(child: ScanProcess, deadlineMs = 25_000): ScanTask {
  let cancelled = false;
  let killTimer: ReturnType<typeof setTimeout> | undefined;
  const running = () => child.exitCode === null && child.signalCode === null;
  const cancel = () => {
    if (cancelled || !running()) return;
    cancelled = true;
    child.kill("SIGINT"); // Stop only this query, never the shared Console server.
    killTimer = setTimeout(() => { if (running()) child.kill("SIGKILL"); }, 1500);
  };
  const deadline = setTimeout(cancel, deadlineMs);
  const promise = (async () => {
    try {
      const [stdout, , exit] = await Promise.all([
        readBounded(child.stdout), readBounded(child.stderr, 16 * 1024), child.exited,
      ]);
      if (cancelled) throw new Error("SCAN_CANCELLED_OR_TIMED_OUT");
      if (exit !== 0 && exit !== 2) throw new Error("SCAN_PROCESS_FAILED");
      return parseReport(stdout);
    } catch (error) {
      cancel();
      throw error;
    } finally {
      clearTimeout(deadline);
      await child.exited;
      if (killTimer) clearTimeout(killTimer);
    }
  })();
  return { promise, cancel };
}
