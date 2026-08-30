import type { ToolPaths } from "./config.js";
import { DebugSession } from "./debug-session.js";
import type { Board, MonitorLayout, TelemetrySnapshot } from "./model.js";

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export interface MonitorCallbacks {
  onState: (state: "connecting" | "live" | "paused" | "reconnecting" | "error", detail: string) => void;
  onSnapshot: (snapshot: TelemetrySnapshot) => void;
}

export class HardwareMonitor {
  private session: DebugSession | undefined;
  private generation = 0;
  private paused = false;
  private stopped = false;

  constructor(
    private readonly board: Board,
    private readonly tools: ToolPaths,
    private readonly layout: MonitorLayout,
    private intervalMs: number,
    private readonly callbacks: MonitorCallbacks,
    private readonly debugPort?: number,
    private readonly expectedFlashPrefix?: Uint8Array,
  ) {}

  start(): void {
    this.stopped = false;
    const generation = ++this.generation;
    void this.run(generation);
  }

  togglePause(): boolean {
    return this.setPaused(!this.paused);
  }

  setPaused(paused: boolean): boolean {
    this.paused = paused;
    this.callbacks.onState(
      this.paused ? "paused" : "live",
      this.paused ? "sampling paused; MCU is running" : "sampling resumed",
    );
    return this.paused;
  }

  setInterval(intervalMs: number): void {
    this.intervalMs = intervalMs;
  }

  async reconnect(): Promise<void> {
    if (this.stopped) return;
    this.callbacks.onState("reconnecting", "closing debugger session");
    const generation = ++this.generation;
    const old = this.session;
    this.session = undefined;
    if (old) await old.close();
    this.paused = false;
    void this.run(generation);
  }

  async stop(): Promise<void> {
    this.stopped = true;
    this.generation += 1;
    const session = this.session;
    this.session = undefined;
    if (session) await session.close();
  }

  private async run(generation: number): Promise<void> {
    const session = new DebugSession(
      this.board,
      this.tools,
      this.layout,
      this.debugPort,
      this.expectedFlashPrefix,
    );
    this.session = session;
    try {
      this.callbacks.onState("connecting", "st-util + GDB; MCU will restart once");
      await session.start();
      if (generation !== this.generation || this.stopped) {
        await session.close();
        return;
      }
      this.callbacks.onState("live", "debugger attached; MCU running between samples");

      while (generation === this.generation && !this.stopped) {
        if (this.paused) {
          await delay(50);
          continue;
        }
        const startedAt = Date.now();
        const snapshot = await session.sample();
        this.callbacks.onSnapshot(snapshot);
        const remaining = this.intervalMs - (Date.now() - startedAt);
        if (remaining > 0) await delay(remaining);
      }
    } catch (error) {
      if (generation === this.generation && !this.stopped) {
        const detail = error instanceof Error ? error.message : String(error);
        this.callbacks.onState("error", detail);
      }
    } finally {
      if (this.session === session) this.session = undefined;
      await session.close();
    }
  }
}
