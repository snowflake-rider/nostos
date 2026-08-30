import type { MonitorControl, WebMonitorState } from "../../src/web-contract";

export async function fetchMonitorState(): Promise<WebMonitorState> {
  const response = await fetch("/api/state", { cache: "no-store" });
  if (!response.ok) throw new Error(`State request failed (${response.status})`);
  return (await response.json()) as WebMonitorState;
}

export function subscribeMonitorState(
  onState: (state: WebMonitorState) => void,
  onConnection: (connected: boolean) => void,
): () => void {
  const source = new EventSource("/api/stream");
  source.onopen = () => onConnection(true);
  source.onmessage = (event) => {
    onState(JSON.parse(event.data) as WebMonitorState);
    onConnection(true);
  };
  source.onerror = () => onConnection(false);
  return () => source.close();
}

export async function sendControl(control: MonitorControl): Promise<WebMonitorState> {
  const response = await fetch("/api/control", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(control),
  });
  const body = (await response.json()) as WebMonitorState | { error?: string };
  if (!response.ok) {
    throw new Error("error" in body && body.error ? body.error : `Control failed (${response.status})`);
  }
  return body as WebMonitorState;
}
