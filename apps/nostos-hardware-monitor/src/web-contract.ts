import type { Board, TelemetrySnapshot } from "./model.js";

export type MonitorPhase =
  | "connecting"
  | "live"
  | "paused"
  | "reconnecting"
  | "error";

export type EventLevel = "info" | "warn" | "error";

export interface MonitorEvent {
  id: string;
  at: number;
  level: EventLevel;
  message: string;
}

export interface WebBoardState {
  board: Board;
  label: string;
  phase: MonitorPhase;
  detail: string;
  snapshot?: TelemetrySnapshot;
  previousSnapshot?: TelemetrySnapshot;
  events: MonitorEvent[];
  sampleCount: number;
  droppedSamples: number;
}

export interface WebMonitorState {
  version: 1;
  intervalMs: number;
  paused: boolean;
  connectedCount: number;
  updatedAt: number;
  boards: WebBoardState[];
}

export type MonitorControl =
  | { action: "pause"; paused: boolean }
  | { action: "reconnect"; boardId?: string }
  | { action: "interval"; intervalMs: number };
