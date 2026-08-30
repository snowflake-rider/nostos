import type { ToolPaths } from "./config.js";
import { createDemoSnapshot } from "./demo.js";
import { HardwareMonitor } from "./monitor.js";
import type { Board, MonitorLayout, TelemetrySnapshot } from "./model.js";
import { detectTelemetryEvents } from "./telemetry-events.js";
import type {
  EventLevel,
  MonitorPhase,
  WebBoardState,
  WebMonitorState,
} from "./web-contract.js";

type StateListener = (state: WebMonitorState) => void;

interface RuntimeBoard extends WebBoardState {
  monitor?: HardwareMonitor;
  demoTick: number;
}

export interface WebRuntimeOptions {
  boards: Board[];
  intervalMs: number;
  demo: boolean;
  tools?: ToolPaths;
  layout?: MonitorLayout;
  expectedFlashPrefix?: Uint8Array;
}

export class WebMonitorRuntime {
  private intervalMs: number;
  private paused = false;
  private readonly boards: RuntimeBoard[];
  private readonly listeners = new Set<StateListener>();
  private eventSequence = 0;
  private demoTimer: ReturnType<typeof setInterval> | undefined;

  constructor(private readonly options: WebRuntimeOptions) {
    this.intervalMs = options.intervalMs;
    this.boards = options.boards.slice(0, 3).map((board, index) => ({
      board,
      label: `Board ${index + 1}`,
      phase: "connecting",
      detail: options.demo ? "demo source" : "waiting for debugger",
      events: [],
      sampleCount: 0,
      droppedSamples: 0,
      demoTick: index * 3,
    }));
  }

  subscribe(listener: StateListener): () => void {
    this.listeners.add(listener);
    listener(this.getState());
    return () => this.listeners.delete(listener);
  }

  getState(): WebMonitorState {
    return {
      version: 1,
      intervalMs: this.intervalMs,
      paused: this.paused,
      connectedCount: this.boards.filter((entry) => entry.phase === "live").length,
      updatedAt: Date.now(),
      boards: this.boards.map(({ monitor: _monitor, demoTick: _demoTick, ...entry }) => ({
        ...entry,
        events: [...entry.events],
      })),
    };
  }

  start(): void {
    if (this.options.demo) {
      for (const entry of this.boards) {
        entry.phase = "live";
        entry.detail = "demo data · no hardware access";
        this.acceptSnapshot(entry, createDemoSnapshot(entry.demoTick));
      }
      this.startDemoTimer();
      this.emit();
      return;
    }

    if (!this.options.tools || !this.options.layout) {
      throw new Error("hardware runtime requires debugger tools and monitor layout");
    }
    this.boards.forEach((entry, index) => {
      const monitor = new HardwareMonitor(
        entry.board,
        this.options.tools!,
        this.options.layout!,
        this.intervalMs,
        {
          onState: (phase, detail) => this.acceptPhase(entry, phase, detail),
          onSnapshot: (snapshot) => this.acceptSnapshot(entry, snapshot),
        },
        45100 + index,
        this.options.expectedFlashPrefix,
      );
      entry.monitor = monitor;
      monitor.start();
    });
  }

  setPaused(paused: boolean): void {
    this.paused = paused;
    for (const entry of this.boards) {
      if (this.options.demo) {
        entry.phase = paused ? "paused" : "live";
        entry.detail = paused ? "sampling paused" : "demo data · no hardware access";
      } else {
        entry.monitor?.setPaused(paused);
      }
      this.addEvent(entry, "info", paused ? "Sampling paused" : "Sampling resumed");
    }
    this.emit();
  }

  setInterval(intervalMs: number): void {
    if (!Number.isInteger(intervalMs) || intervalMs < 100 || intervalMs > 2000) {
      throw new Error("intervalMs must be between 100 and 2000");
    }
    this.intervalMs = intervalMs;
    for (const entry of this.boards) entry.monitor?.setInterval(intervalMs);
    if (this.options.demo) this.startDemoTimer();
    this.emit();
  }

  async reconnect(boardId?: string): Promise<void> {
    const targets = this.boards.filter((entry) => !boardId || entry.board.id === boardId);
    if (targets.length === 0) throw new Error(`unknown board: ${boardId}`);
    if (this.options.demo) {
      for (const entry of targets) {
        entry.phase = "live";
        entry.detail = "demo reconnected";
        this.addEvent(entry, "info", "Reconnected");
      }
      this.emit();
      return;
    }
    await Promise.all(targets.map((entry) => entry.monitor?.reconnect()));
  }

  async stop(): Promise<void> {
    if (this.demoTimer) clearInterval(this.demoTimer);
    this.demoTimer = undefined;
    await Promise.all(this.boards.map((entry) => entry.monitor?.stop()));
  }

  private startDemoTimer(): void {
    if (this.demoTimer) clearInterval(this.demoTimer);
    this.demoTimer = setInterval(() => {
      if (this.paused) return;
      for (const entry of this.boards) {
        entry.demoTick += 1;
        this.acceptSnapshot(entry, createDemoSnapshot(entry.demoTick));
      }
    }, this.intervalMs);
  }

  private acceptPhase(entry: RuntimeBoard, phase: MonitorPhase, detail: string): void {
    entry.phase = phase;
    entry.detail = detail;
    if (phase === "error") {
      entry.snapshot = undefined;
      entry.previousSnapshot = undefined;
      entry.droppedSamples += 1;
      this.addEvent(entry, "error", detail);
    } else if (phase === "live" && entry.sampleCount === 0) {
      this.addEvent(entry, "info", "Debugger attached");
    }
    this.emit();
  }

  private acceptSnapshot(entry: RuntimeBoard, snapshot: TelemetrySnapshot): void {
    for (const event of detectTelemetryEvents(entry.snapshot, snapshot)) {
      this.addEvent(entry, event.level, event.message);
    }
    entry.previousSnapshot = entry.snapshot;
    entry.snapshot = snapshot;
    entry.sampleCount += 1;
    entry.phase = this.paused ? "paused" : "live";
    entry.detail = `sample ${new Date(snapshot.collectedAtMs).toLocaleTimeString("ko-KR", { hour12: false })}`;
    this.emit();
  }

  private addEvent(entry: RuntimeBoard, level: EventLevel, message: string): void {
    entry.events.unshift({
      id: `${entry.board.id}-${++this.eventSequence}`,
      at: Date.now(),
      level,
      message,
    });
    entry.events = entry.events.slice(0, 8);
  }

  private emit(): void {
    const state = this.getState();
    for (const listener of this.listeners) listener(state);
  }
}
