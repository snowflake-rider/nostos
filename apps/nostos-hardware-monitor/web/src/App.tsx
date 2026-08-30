import { useState } from "react";

import { BoardPanel } from "./BoardPanel";
import { ActivityIcon, LinkIcon, PauseIcon, PlayIcon, RefreshIcon } from "./icons";
import { useMonitor } from "./use-monitor";

const INTERVALS = [100, 250, 500, 1000] as const;

function formatTime(value: number | undefined): string {
  if (!value) return "—";
  return new Intl.DateTimeFormat("en-GB", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  }).format(value);
}

export function App() {
  const { state, streamConnected, error, controlPending, control } = useMonitor();
  const [reconnectingBoard, setReconnectingBoard] = useState<string>();
  const boards = state?.boards ?? [];
  const sampleCount = boards.reduce((sum, entry) => sum + entry.sampleCount, 0);
  const droppedSamples = boards.reduce((sum, entry) => sum + entry.droppedSamples, 0);

  const reconnect = async (boardId?: string) => {
    setReconnectingBoard(boardId ?? "all");
    await control({ action: "reconnect", ...(boardId ? { boardId } : {}) });
    setReconnectingBoard(undefined);
  };

  return (
    <main className="app-shell">
      <header className="app-header">
        <div className="title-block">
          <ActivityIcon />
          <div>
            <h1>NOSTOS Hardware Monitor</h1>
            <p>STM32 · live SWD diagnostics</p>
          </div>
        </div>
        <div className="global-controls">
          <div className={`connection-summary ${state?.connectedCount === boards.length && boards.length > 0 ? "healthy" : "warning"}`}>
            <i />
            <strong>{state?.connectedCount ?? 0} / {boards.length || 3}</strong>
            <span>connected</span>
          </div>
          <label className="interval-control">
            <span>Sampling</span>
            <select
              aria-label="Sampling interval"
              value={state?.intervalMs ?? 250}
              disabled={!state || controlPending}
              onChange={(event) => void control({ action: "interval", intervalMs: Number(event.target.value) })}
            >
              {INTERVALS.map((value) => <option value={value} key={value}>{value} ms</option>)}
            </select>
          </label>
          <button
            className="control-button primary"
            type="button"
            disabled={!state || controlPending}
            onClick={() => void control({ action: "pause", paused: !state?.paused })}
          >
            {state?.paused ? <PlayIcon /> : <PauseIcon />}
            {state?.paused ? "Resume all" : "Pause all"}
          </button>
          <button
            className="control-button"
            type="button"
            disabled={!state || controlPending || Boolean(reconnectingBoard)}
            onClick={() => void reconnect()}
          >
            <RefreshIcon />
            Reconnect all
          </button>
        </div>
      </header>

      {error ? <div className="global-error" role="alert">{error}</div> : null}

      <section className="board-grid" aria-label="Connected STM32 boards">
        {boards.length > 0 ? boards.map((board) => (
          <BoardPanel
            key={board.board.id}
            state={board}
            reconnecting={reconnectingBoard === board.board.id || reconnectingBoard === "all"}
            onReconnect={(boardId) => void reconnect(boardId)}
          />
        )) : [1, 2, 3].map((number) => (
          <article className="board-panel board-skeleton" key={number}>
            <header className="board-header"><h2>Board {number}</h2></header>
            <div className="board-empty"><span className="loader" /><strong>Loading monitor</strong></div>
          </article>
        ))}
      </section>

      <footer className="status-rail">
        <div className={streamConnected ? "healthy" : "danger"}><LinkIcon /><span>Local stream</span><strong>{streamConnected ? "Connected" : "Reconnecting"}</strong></div>
        <div><span>Sampling</span><strong>{state?.intervalMs ?? 250} ms</strong></div>
        <div><span>Last update</span><strong>{formatTime(state?.updatedAt)}</strong></div>
        <div><span>Samples</span><strong>{sampleCount.toLocaleString()}</strong></div>
        <div className={droppedSamples ? "danger" : ""}><span>Dropped</span><strong>{droppedSamples}</strong></div>
        <div className="activity-bars" aria-label="Live activity">
          {Array.from({ length: 18 }, (_, index) => <i key={index} style={{ height: `${6 + ((sampleCount + index * 7) % 17)}px` }} />)}
        </div>
      </footer>
    </main>
  );
}
