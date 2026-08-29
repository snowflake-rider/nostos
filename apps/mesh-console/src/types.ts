export type Board = "D6" | "B6" | "76";
export type Command =
  "status" | "on" | "off" | "on-unack" | "off-unack" | "tx-low" | "tx-normal";
export interface NodeState {
  board: Board;
  serial: string;
  name: string;
  detected: boolean;
  path: string | null;
  phase: "disconnected" | "connecting" | "verifying" | "connected" | "error";
  error: string | null;
  status: Record<string, string> | null;
  status_at: number | null;
  fresh: boolean;
  last_power_request: string | null;
}
export interface BridgeState {
  type: "state";
  instance: string;
  mode: "live" | "test";
  nodes: NodeState[];
  seq: number;
  dropped: number;
  scan_error: string | null;
  poll_seconds: number;
  limit: number;
  storage?: StorageState;
}
export interface StorageState {
  enabled: boolean;
  directory: string | null;
  file: string | null;
  saved: number;
  pending: number;
  missed: number;
  error: string | null;
}
export interface MessageEvent {
  id: number;
  hex: string;
  symbol: string | null;
  label: string;
  known: boolean;
  stage: string;
  source: string | null;
  result: string | null;
}
export interface LogRow {
  id: number;
  time: string;
  board: Board;
  direction: "rx" | "tx" | "system";
  text: string;
  category: string;
  level: "info" | "warning" | "error";
  uptime_ms: number | null;
  event?: MessageEvent | null;
}
export type WireMessage =
  | BridgeState
  | { type: "log"; log: LogRow }
  | { type: "snapshot"; state: BridgeState; logs: LogRow[] };
export const BOARDS: Board[] = ["D6", "B6", "76"];
export function isOpen(node: NodeState) {
  return ["connecting", "verifying", "connected"].includes(node.phase);
}
export function phaseLabel(node: NodeState, online: boolean) {
  if (!online) return "서버 연결 끊김";
  if (node.phase === "error") return "연결 오류";
  if (node.phase === "connecting") return "연결 중";
  if (node.phase === "verifying") return "콘솔 확인 중";
  if (node.phase === "connected") return node.fresh ? "연결됨" : "상태 오래됨";
  return node.detected ? "USB 감지 · 미연결" : "USB 미감지";
}
