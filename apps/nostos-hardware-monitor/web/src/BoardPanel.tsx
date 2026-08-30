import type { WebBoardState } from "../../src/web-contract";
import { RefreshIcon } from "./icons";

const RESULT_NAMES = [
  "OK", "EMPTY", "BAD_ARGUMENT", "BAD_LENGTH", "BAD_VALUE", "TOO_LARGE",
  "UNSUPPORTED_VERSION", "UNSUPPORTED_TYPE", "BAD_CRC", "TIMEOUT", "UNAUTHORIZED",
  "SESSION_REQUIRED", "STALE", "DUPLICATE", "FULL", "NOT_READY", "EXPIRED",
  "EXHAUSTED", "CONFLICT", "IO_ERROR",
] as const;

const AUDIO_NAMES = [
  "OK", "INVALID_ARGUMENT", "DREQ_TIMEOUT", "SPI_ERROR", "MODE_MISMATCH",
  "REGISTER_MISMATCH", "BUSY",
] as const;

function enumName(values: readonly string[], value: number): string {
  return values[value] ?? `UNKNOWN ${value}`;
}

function formatNumber(value: number): string {
  return new Intl.NumberFormat("en-US").format(value);
}

function formatTime(value: number): string {
  return new Intl.DateTimeFormat("en-GB", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    fractionalSecondDigits: 3,
    hour12: false,
  }).format(value);
}

function Metric({ label, value, tone }: { label: string; value: string | number; tone?: string }) {
  return (
    <div className={`metric ${tone ?? ""}`}>
      <span>{label}</span>
      <strong>{value}</strong>
    </div>
  );
}

function MismatchIcon() {
  return (
    <svg aria-hidden="true" viewBox="0 0 24 24">
      <path d="M12 3 2.7 20h18.6L12 3Z" />
      <path d="M12 8.2v5.6M12 17.1v.2" />
    </svg>
  );
}

interface BoardPanelProps {
  state: WebBoardState;
  reconnecting: boolean;
  onReconnect: (boardId: string) => void;
}

export function BoardPanel({ state, reconnecting, onReconnect }: BoardPanelProps) {
  const snapshot = state.snapshot;
  const previous = state.previousSnapshot;
  const heartbeatDelta = snapshot && previous
    ? (snapshot.rtos.inputHeartbeat - previous.rtos.inputHeartbeat) >>> 0
    : 0;
  const phaseTone = state.phase === "error" ? "danger" : state.phase === "live" ? "healthy" : "warning";
  const isError = state.phase === "error";

  return (
    <article className={`board-panel phase-${state.phase}`} aria-label={`${state.label} monitor`}>
      <header className="board-header">
        <div>
          <h2>{state.label}</h2>
          <p title={state.board.id}>{state.board.id}</p>
        </div>
        <div className="board-actions">
          <span className={`phase ${phaseTone}`}><i />{state.phase.toUpperCase()}</span>
          <button
            className="icon-button"
            type="button"
            title={`Reconnect ${state.label}`}
            aria-label={`Reconnect ${state.label}`}
            disabled={reconnecting}
            onClick={() => onReconnect(state.board.id)}
          >
            <RefreshIcon />
          </button>
        </div>
      </header>

      {!snapshot ? (
        <div
          className={`board-empty ${isError ? "mismatch" : "connecting"}`}
          role={isError ? "alert" : "status"}
          aria-live="polite"
        >
          {isError ? (
            <>
              <div className="empty-state-heading">
                <span className="mismatch-icon"><MismatchIcon /></span>
                <div>
                  <strong>Firmware / ELF mismatch</strong>
                  <p>Load firmware built from the same source as this monitor, then reconnect.</p>
                </div>
              </div>
              <pre className="mismatch-detail"><code>{state.detail}</code></pre>
              <p className="telemetry-safety">
                <strong>Telemetry hidden for safety.</strong>
                Values will appear only after matching firmware is loaded. This monitor does not flash or modify the board.
              </p>
              <button
                className="empty-action-button"
                type="button"
                disabled={reconnecting}
                onClick={() => onReconnect(state.board.id)}
              >
                <RefreshIcon />
                {reconnecting ? "Reconnecting…" : "Reconnect debugger"}
              </button>
            </>
          ) : (
            <>
              <span className="loader" />
              <strong>Connecting debugger</strong>
              <p className="connecting-detail">{state.detail}</p>
              <small>Telemetry will appear automatically after a safe snapshot is verified.</small>
            </>
          )}
        </div>
      ) : (
        <>
          <section className="monitor-section buttons-section">
            <h3>Buttons</h3>
            <div className="button-grid">
              {snapshot.buttons.slice(0, 4).map((button) => {
                const reset = button.name === "BTN4";
                return (
                  <div
                    className={`button-cell ${button.stablePressed ? "pressed" : ""} ${reset ? "reset" : ""}`}
                    key={button.name}
                  >
                    <strong>{button.name}{reset ? <small>RESET</small> : null}</strong>
                    <dl>
                      <div><dt>Raw</dt><dd>{Number(button.rawPressed)}</dd></div>
                      <div><dt>Debounced</dt><dd>{Number(button.stablePressed)}</dd></div>
                      <div><dt>Armed</dt><dd>{Number(button.armed)}</dd></div>
                    </dl>
                  </div>
                );
              })}
            </div>
          </section>

          <section className="monitor-section health-section">
            <h3>FreeRTOS</h3>
            <div className="health-grid">
              <Metric label="Scheduler" value={snapshot.schedulerRunning ? "Running" : "Stopped"} tone={snapshot.schedulerRunning ? "healthy" : "danger"} />
              <Metric label="Heartbeat" value={`+${heartbeatDelta}`} tone={heartbeatDelta > 0 ? "healthy" : "warning"} />
            </div>
          </section>

          <section className="monitor-section queue-section">
            <h3>Queues</h3>
            <div className="metric-grid five">
              <Metric label="Queued" value={formatNumber(snapshot.rtos.queued)} />
              <Metric label="Dispatched" value={formatNumber(snapshot.rtos.dispatched)} />
              <Metric label="Full" value={snapshot.rtos.queueFull} tone={snapshot.rtos.queueFull ? "danger" : ""} />
              <Metric label="Expired" value={snapshot.rtos.expired} tone={snapshot.rtos.expired ? "warning" : ""} />
              <Metric label="Resets" value={snapshot.rtos.resets} tone={snapshot.rtos.resets ? "warning" : ""} />
            </div>
          </section>

          <section className="monitor-section output-section">
            <h3>Outputs</h3>
            <div className="output-grid">
              <div className="metric rgb-metric">
                <span>RGB</span>
                <strong className="rgb-lights" aria-label={`RGB ${snapshot.outputs.rgbRed ? "red " : ""}${snapshot.outputs.rgbGreen ? "green " : ""}${snapshot.outputs.rgbBlue ? "blue" : "off"}`}>
                  <i className={`red ${snapshot.outputs.rgbRed ? "active" : ""}`}>R</i>
                  <i className={`green ${snapshot.outputs.rgbGreen ? "active" : ""}`}>G</i>
                  <i className={`blue ${snapshot.outputs.rgbBlue ? "active" : ""}`}>B</i>
                </strong>
              </div>
              <Metric label="Audio" value={snapshot.outputs.audioPlaying ? "ON" : enumName(AUDIO_NAMES, snapshot.outputs.audioStatus)} tone={snapshot.outputs.audioPlaying ? "healthy" : ""} />
              <Metric label="Buzzer" value={snapshot.outputs.buzzerActive || snapshot.outputs.buzzerPin ? "ON" : "OFF"} tone={snapshot.outputs.buzzerActive ? "warning" : ""} />
            </div>
          </section>

          <section className="monitor-section transport-section">
            <h3>Transport</h3>
            <div className="transport-grid">
              <Metric label="UART TX" value={formatNumber(snapshot.transport.tx)} />
              <Metric label="UART RX" value={formatNumber(snapshot.transport.rx)} />
              <Metric
                label="Protocol v2"
                value={enumName(RESULT_NAMES, snapshot.protocolStatus)}
                tone={snapshot.protocolStatus === 0 ? "healthy" : snapshot.protocolStatus === 15 ? "warning" : "danger"}
              />
            </div>
            <p className="transport-detail">Local {snapshot.transport.localRouted} · Remote {snapshot.transport.remoteRouted} · Dropped {snapshot.transport.dropped}</p>
          </section>

          <section className="monitor-section events-section">
            <h3>Live events</h3>
            {state.events.length === 0 ? <p className="no-events">No changes yet</p> : (
              <ol className="event-list">
                {state.events.slice(0, 5).map((event) => (
                  <li key={event.id} data-level={event.level}>
                    <time>{formatTime(event.at)}</time>
                    <span>{event.message}</span>
                    <em>{event.level}</em>
                  </li>
                ))}
              </ol>
            )}
          </section>
        </>
      )}
    </article>
  );
}
