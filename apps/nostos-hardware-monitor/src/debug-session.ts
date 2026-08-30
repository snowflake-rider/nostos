import { decodeTelemetry, type Board, type MemoryBlock, type MonitorLayout, type TelemetrySnapshot } from "./model.js";
import { REPO_ROOT, STM32_ELF_PATH, type ToolPaths } from "./config.js";

const STM32_FLASH_BASE = 0x08000000;
const FLASH_READ_CHUNK_LENGTH = 1024;

export interface ByteMismatch {
  offset: number;
  expected: number | undefined;
  actual: number | undefined;
}

export function findFirstByteMismatch(
  expected: Uint8Array,
  actual: Uint8Array,
): ByteMismatch | undefined {
  const commonLength = Math.min(expected.length, actual.length);
  for (let offset = 0; offset < commonLength; offset += 1) {
    if (expected[offset] !== actual[offset]) {
      return { offset, expected: expected[offset], actual: actual[offset] };
    }
  }
  if (expected.length !== actual.length) {
    return {
      offset: commonLength,
      expected: expected[commonLength],
      actual: actual[commonLength],
    };
  }
  return undefined;
}

interface PendingCommand {
  resolve: (line: string) => void;
  reject: (error: Error) => void;
  timeout: ReturnType<typeof setTimeout>;
}

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function pumpLines(
  stream: ReadableStream<Uint8Array>,
  onLine: (line: string) => void,
): Promise<void> {
  const reader = stream.getReader();
  const decoder = new TextDecoder();
  let pending = "";
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      pending += decoder.decode(value, { stream: true });
      const lines = pending.split(/\r?\n/);
      pending = lines.pop() ?? "";
      for (const line of lines) onLine(line);
    }
    pending += decoder.decode();
    if (pending.length > 0) onLine(pending);
  } finally {
    reader.releaseLock();
  }
}

function withTimeout<T>(promise: Promise<T>, ms: number, label: string): Promise<T> {
  let timeout: ReturnType<typeof setTimeout> | undefined;
  const timed = new Promise<T>((_, reject) => {
    timeout = setTimeout(() => reject(new Error(`${label} timed out after ${ms} ms`)), ms);
  });
  return Promise.race([promise, timed]).finally(() => {
    if (timeout) clearTimeout(timeout);
  });
}

class MiClient {
  private process: Bun.PipedSubprocess | undefined;
  private nextToken = 1;
  private pending = new Map<number, PendingCommand>();
  private stoppedWaiters: Array<() => void> = [];
  private state: "starting" | "stopped" | "running" | "closed" = "starting";
  private recentOutput: string[] = [];

  constructor(
    private readonly gdbPath: string,
    private readonly elfPath: string,
  ) {}

  async start(): Promise<void> {
    this.process = Bun.spawn(
      [this.gdbPath, "--quiet", "--nx", "--interpreter=mi2", this.elfPath],
      {
        cwd: REPO_ROOT,
        stdin: "pipe",
        stdout: "pipe",
        stderr: "pipe",
      },
    );
    void pumpLines(this.process.stdout, (line) => this.handleLine(line));
    void pumpLines(this.process.stderr, (line) => this.remember(line));
    void this.process.exited.then((code) => {
      this.state = "closed";
      const error = new Error(
        `GDB exited with code ${code}: ${this.recentOutput.slice(-5).join(" | ")}`,
      );
      for (const entry of this.pending.values()) {
        clearTimeout(entry.timeout);
        entry.reject(error);
      }
      this.pending.clear();
    });
    await this.command("-gdb-set mi-async on");
  }

  private remember(line: string): void {
    if (line.trim().length === 0) return;
    this.recentOutput.push(line.trim());
    if (this.recentOutput.length > 30) this.recentOutput.shift();
  }

  private handleLine(line: string): void {
    this.remember(line);
    if (line.startsWith("*stopped")) {
      this.state = "stopped";
      const waiters = this.stoppedWaiters.splice(0);
      for (const resolve of waiters) resolve();
      return;
    }
    if (line.startsWith("*running")) {
      this.state = "running";
      return;
    }

    const match = line.match(/^(\d+)\^(done|running|connected|error|exit)(?:,(.*))?$/);
    if (!match) return;
    const token = Number.parseInt(match[1]!, 10);
    const resultClass = match[2]!;
    const entry = this.pending.get(token);
    if (!entry) return;
    clearTimeout(entry.timeout);
    this.pending.delete(token);
    if (resultClass === "error") {
      entry.reject(new Error(`GDB command failed: ${line}`));
    } else {
      if (resultClass === "running") this.state = "running";
      entry.resolve(line);
    }
  }

  command(command: string, timeoutMs = 5000): Promise<string> {
    if (!this.process || this.state === "closed") {
      return Promise.reject(new Error("GDB is not running"));
    }
    const token = this.nextToken++;
    return new Promise<string>((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pending.delete(token);
        reject(new Error(`GDB command timed out: ${command}`));
      }, timeoutMs);
      this.pending.set(token, { resolve, reject, timeout });
      this.process!.stdin.write(`${token}${command}\n`);
      this.process!.stdin.flush();
    });
  }

  async connect(port: number): Promise<void> {
    await this.command(`-target-select extended-remote 127.0.0.1:${port}`, 10000);
    if (this.state !== "stopped") await this.waitUntilStopped();
  }

  private waitUntilStopped(): Promise<void> {
    if (this.state === "stopped") return Promise.resolve();
    return withTimeout(
      new Promise((resolve) => this.stoppedWaiters.push(resolve)),
      5000,
      "waiting for MCU halt",
    );
  }

  async interrupt(): Promise<void> {
    if (this.state === "stopped") return;
    const stopped = this.waitUntilStopped();
    await this.command("-exec-interrupt");
    await stopped;
  }

  async resume(): Promise<void> {
    if (this.state === "running") return;
    await this.command("-exec-continue");
    this.state = "running";
  }

  async readMemory(address: number, length: number): Promise<MemoryBlock> {
    const line = await this.command(
      `-data-read-memory-bytes 0x${address.toString(16)} ${length}`,
    );
    const match = line.match(/contents="([0-9a-fA-F]+)"/);
    if (!match) throw new Error(`GDB memory response did not contain bytes: ${line}`);
    const hex = match[1]!;
    if (hex.length !== length * 2) {
      throw new Error(`GDB returned ${hex.length / 2} bytes, expected ${length}`);
    }
    const bytes = new Uint8Array(length);
    for (let index = 0; index < length; index += 1) {
      bytes[index] = Number.parseInt(hex.slice(index * 2, index * 2 + 2), 16);
    }
    return { address, bytes };
  }

  async close(): Promise<void> {
    const process = this.process;
    if (!process || this.state === "closed") return;
    try {
      if (this.state === "stopped") await this.resume();
      await this.command("-target-disconnect", 2000);
      await this.command("-gdb-exit", 2000).catch(() => undefined);
      await withTimeout(process.exited, 2000, "GDB exit").catch(() => undefined);
    } finally {
      if (process.exitCode === null) process.kill("SIGTERM");
      this.state = "closed";
      this.process = undefined;
    }
  }
}

export class DebugSession {
  private stUtil: Bun.ReadableSubprocess | undefined;
  private mi: MiClient | undefined;
  private port = 0;

  constructor(
    readonly board: Board,
    private readonly tools: ToolPaths,
    private readonly layout: MonitorLayout,
    private readonly requestedPort?: number,
    private readonly expectedFlashPrefix?: Uint8Array,
  ) {}

  async start(): Promise<void> {
    this.port = this.requestedPort ?? 44000 + Math.floor(Math.random() * 10000);
    let readyResolve: (() => void) | undefined;
    let readyReject: ((error: Error) => void) | undefined;
    const ready = new Promise<void>((resolve, reject) => {
      readyResolve = resolve;
      readyReject = reject;
    });
    const recent: string[] = [];
    const handleStUtilLine = (line: string): void => {
      recent.push(line.trim());
      if (recent.length > 20) recent.shift();
      if (line.includes("Listening at")) readyResolve?.();
    };

    this.stUtil = Bun.spawn(
      [
        this.tools.stUtil,
        "--serial",
        this.board.serial,
        "--multi",
        "-p",
        String(this.port),
      ],
      { cwd: REPO_ROOT, stdin: "ignore", stdout: "pipe", stderr: "pipe" },
    );
    void pumpLines(this.stUtil.stdout, handleStUtilLine);
    void pumpLines(this.stUtil.stderr, handleStUtilLine);
    void this.stUtil.exited.then((code) => {
      readyReject?.(
        new Error(`st-util exited with code ${code}: ${recent.slice(-5).join(" | ")}`),
      );
    });

    try {
      await withTimeout(ready, 5000, `st-util for ${this.board.id}`);
      this.mi = new MiClient(this.tools.gdb, STM32_ELF_PATH);
      await this.mi.start();
      await this.mi.connect(this.port);
      await this.verifyFirmware();
      await this.mi.resume();
      await delay(250);
    } catch (error) {
      await this.close();
      throw error;
    }
  }

  private async verifyFirmware(): Promise<void> {
    const expected = this.expectedFlashPrefix;
    const mi = this.mi;
    if (!expected || !mi) return;

    const actual = new Uint8Array(expected.length);
    for (let offset = 0; offset < expected.length; offset += FLASH_READ_CHUNK_LENGTH) {
      const length = Math.min(FLASH_READ_CHUNK_LENGTH, expected.length - offset);
      const block = await mi.readMemory(STM32_FLASH_BASE + offset, length);
      actual.set(block.bytes, offset);
    }

    const mismatch = findFirstByteMismatch(expected, actual);
    if (!mismatch) return;
    const address = STM32_FLASH_BASE + mismatch.offset;
    const byte = (value: number | undefined) =>
      value === undefined ? "missing" : `0x${value.toString(16).padStart(2, "0")}`;
    throw new Error(
      `firmware verification failed for ${this.board.id}: target flash differs from current ELF ` +
        `at 0x${address.toString(16).padStart(8, "0")} ` +
        `(expected ${byte(mismatch.expected)}, got ${byte(mismatch.actual)}); telemetry rejected`,
    );
  }

  async sample(): Promise<TelemetrySnapshot> {
    const mi = this.mi;
    if (!mi) throw new Error("debug session is not connected");
    await mi.interrupt();
    try {
      const symbol = (name: string) => {
        const entries = this.layout.symbols.get(name) ?? [];
        if (entries.length !== 1) throw new Error(`missing unique symbol ${name}`);
        return entries[0]!;
      };
      const appEnd = symbol("input_heartbeat").address + symbol("input_heartbeat").size;
      const queueStart = symbol("normal_queue").address;
      const queueEnd = symbol("urgent_queue").address + symbol("urgent_queue").size;

      const blocks = await Promise.all([
        mi.readMemory(this.layout.debugStart, this.layout.debugLength),
        mi.readMemory(this.layout.appStats.address, appEnd - this.layout.appStats.address),
        mi.readMemory(this.layout.protocolStats.address, this.layout.protocolStats.size),
        mi.readMemory(queueStart, queueEnd - queueStart),
        mi.readMemory(symbol("xSchedulerRunning").address, 4),
        mi.readMemory(0x40020010, 8),
        mi.readMemory(0x40020410, 8),
        mi.readMemory(0x40020810, 8),
      ]);
      return decodeTelemetry(this.layout, blocks);
    } finally {
      await mi.resume();
    }
  }

  async close(): Promise<void> {
    const mi = this.mi;
    this.mi = undefined;
    if (mi) await mi.close().catch(() => undefined);

    const stUtil = this.stUtil;
    this.stUtil = undefined;
    if (stUtil && stUtil.exitCode === null) {
      stUtil.kill("SIGINT");
      await withTimeout(stUtil.exited, 1500, "st-util exit").catch(() => {
        if (stUtil.exitCode === null) stUtil.kill("SIGTERM");
      });
    }
  }
}
