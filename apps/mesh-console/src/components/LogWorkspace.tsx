import { useDeferredValue, useEffect, useMemo, useRef, useState } from "react";
import {
  Download,
  Pause,
  Play,
  Search,
  Terminal,
  Trash2,
  X,
} from "lucide-react";
import { BOARDS, type LogRow, type StorageState } from "../types";
import { StorageStatus } from "./StorageStatus";
import {
  downloadLogs,
  filterLogs,
  formatTime,
  previewText,
  type Filters,
} from "../logs";
const HEIGHT = 44;
export function LogWorkspace({
  logs,
  instance,
  storage,
}: {
  logs: LogRow[];
  instance: string;
  storage?: StorageState;
}) {
  const [filters, setFilters] = useState<Filters>({
    board: "all",
    category: "all",
    query: "",
    errors: false,
  });
  const [paused, setPaused] = useState<LogRow[] | null>(null);
  const [clearPoint, setClearPoint] = useState({ instance, id: 0 });
  const [autoScroll, setAutoScroll] = useState(true);
  const [exportOpen, setExportOpen] = useState(false);
  const [selectedRow, setSelectedRow] = useState<LogRow | null>(null);
  const [viewport, setViewport] = useState({ top: 0, height: 500 });
  const scroller = useRef<HTMLDivElement>(null);
  const query = useDeferredValue(filters.query);
  const cutoff = clearPoint.instance === instance ? clearPoint.id : 0;
  const source = paused ?? logs;
  const rows = useMemo(
    () =>
      filterLogs(
        source.filter((row) => row.id > cutoff),
        { ...filters, query },
      ),
    [source, cutoff, filters, query],
  );
  const start = Math.max(0, Math.floor(viewport.top / HEIGHT) - 8);
  const end = Math.min(
    rows.length,
    Math.ceil((viewport.top + viewport.height) / HEIGHT) + 8,
  );
  useEffect(() => {
    const el = scroller.current!;
    const observer = new ResizeObserver(() =>
      setViewport((v) => ({ ...v, height: el.clientHeight })),
    );
    observer.observe(el);
    return () => observer.disconnect();
  }, []);
  useEffect(() => {
    const el = scroller.current!;
    if (autoScroll && !paused) el.scrollTop = el.scrollHeight;
    setViewport((v) => ({ ...v, top: el.scrollTop }));
  }, [rows, autoScroll, paused]);
  function clearView() {
    setClearPoint({ instance, id: logs.at(-1)?.id ?? 0 });
    setPaused(null);
    setSelectedRow(null);
  }
  function save(format: "txt" | "jsonl") {
    downloadLogs(rows, format);
    setExportOpen(false);
  }
  return (
    <main className="log-workspace" aria-label="메시지 로그">
      <div className="log-heading">
        <h1>메시지 로그</h1>
        <p>메시지 ID는 한글로 해석하며 원문도 보존합니다.</p>
      </div>
      <StorageStatus storage={storage} />
      <div className="toolbar">
        <div className="segmented" role="group" aria-label="로그 보드 필터">
          {(["all", ...BOARDS] as const).map((board) => (
            <button
              key={board}
              aria-pressed={filters.board === board}
              onClick={() => setFilters((f) => ({ ...f, board }))}
            >
              {board === "all" ? "전체" : board}
            </button>
          ))}
        </div>
        <label className="search">
          <Search size={17} />
          <input
            aria-label="로그 검색"
            placeholder="로그 검색"
            value={filters.query}
            onChange={(e) =>
              setFilters((f) => ({ ...f, query: e.target.value }))
            }
          />
        </label>
        <button
          className="tool-button"
          aria-label={paused ? "재개" : "일시정지"}
          onClick={() => setPaused(paused ? null : logs.slice())}
          aria-pressed={paused !== null}
          title="표시만 멈추며 USB 수집은 계속됩니다"
        >
          {paused ? <Play size={17} /> : <Pause size={17} />}
          <span>{paused ? "재개" : "일시정지"}</span>
        </button>
        <div className="export-control">
          <button
            className="tool-button"
            aria-label="저장"
            disabled={!rows.length}
            aria-expanded={exportOpen}
            onClick={() => setExportOpen(!exportOpen)}
          >
            <Download size={17} />
            <span>저장</span>
          </button>
          {exportOpen && (
            <div className="export-menu">
              <p>화면 필터 결과 {rows.length.toLocaleString()}줄</p>
              <button onClick={() => save("txt")}>텍스트 (.txt)</button>
              <button onClick={() => save("jsonl")}>JSONL (.jsonl)</button>
              <button onClick={() => setExportOpen(false)}>닫기</button>
            </div>
          )}
        </div>
        <button
          className="icon-button"
          aria-label="화면 로그 지우기"
          title="화면에서만 지웁니다. 서버 기록은 유지됩니다."
          onClick={clearView}
          disabled={!logs.length}
        >
          <Trash2 size={18} />
        </button>
      </div>
      <div className="filter-line">
        <label>
          <span className="sr-only">로그 분류</span>
          <select
            aria-label="로그 분류"
            value={filters.category}
            onChange={(e) =>
              setFilters((f) => ({ ...f, category: e.target.value }))
            }
          >
            {[
              "all",
              "UART RX",
              "MESH TX",
              "MESH RX",
              "ONOFF RX",
              "UART TX",
              "STATUS",
              "COMMAND",
              "ERROR",
              "SYSTEM",
              "OTHER",
            ].map((category) => (
              <option value={category} key={category}>
                {category === "all" ? "전체 이벤트" : category}
              </option>
            ))}
          </select>
        </label>
        <label className="check-label">
          <input
            type="checkbox"
            checked={filters.errors}
            onChange={(e) =>
              setFilters((f) => ({ ...f, errors: e.target.checked }))
            }
          />
          오류만
        </label>
        <label className="check-label auto-scroll">
          <input
            type="checkbox"
            checked={autoScroll}
            onChange={(e) => setAutoScroll(e.target.checked)}
          />
          자동 스크롤
        </label>
      </div>
      {paused && (
        <div className="pause-banner" role="status">
          표시 일시정지 · USB 수집은 계속됩니다.
          <span>
            {Math.max(0, (logs.at(-1)?.id ?? 0) - (paused.at(-1)?.id ?? 0))}줄
            이후 수신
          </span>
        </div>
      )}
      <div className="log-table-head">
        <span>시간</span>
        <span>보드</span>
        <span>구분</span>
        <span>메시지</span>
      </div>
      <div
        className="log-scroll"
        ref={scroller}
        tabIndex={0}
        aria-label="로그 목록"
        onScroll={(e) => {
          const top = e.currentTarget.scrollTop;
          setViewport((v) => ({ ...v, top }));
        }}
      >
        {!rows.length ? (
          <div className="empty-state">
            <Terminal size={30} />
            <h2>
              {logs.length && cutoff === 0
                ? "일치하는 로그가 없습니다"
                : "메시지를 기다리고 있습니다"}
            </h2>
            <p>
              {logs.length && cutoff === 0
                ? "보드, 이벤트 필터 또는 검색어를 확인해 주세요."
                : "왼쪽에서 보드를 연결하면 로그가 여기에 표시됩니다."}
            </p>
          </div>
        ) : (
          <div
            className="virtual-log"
            style={{ height: rows.length * HEIGHT }}
            role="table"
            aria-label={`로그 ${rows.length}줄`}
            aria-rowcount={rows.length}
          >
            <div style={{ transform: `translateY(${start * HEIGHT}px)` }}>
              {rows.slice(start, end).map((row) => (
                <button
                  key={row.id}
                  className={`log-row ${row.level}`}
                  onClick={() => setSelectedRow(row)}
                  aria-label={`${row.board} ${row.category} ${row.event?.label ?? ""} ${row.text}`}
                >
                  <time title={row.time}>{formatTime(row.time)}</time>
                  <span className={`node-label node-${row.board}`}>
                    {row.board}
                  </span>
                  <span
                    className={`log-category category-${row.category.replace(" ", "-").toLowerCase()}`}
                  >
                    {row.category}
                  </span>
                  <span className="log-text" title={row.text}>
                    {row.event && (
                      <strong className="event-label">
                        {row.event.label} ·{" "}
                      </strong>
                    )}
                    {previewText(row.text)}
                  </span>
                </button>
              ))}
            </div>
          </div>
        )}
      </div>
      {selectedRow && (
        <section className="log-detail" aria-label="로그 원문">
          <div>
            <strong>
              {selectedRow.board} · {selectedRow.category}
            </strong>
            <button
              className="icon-button"
              aria-label="로그 원문 닫기"
              onClick={() => setSelectedRow(null)}
            >
              <X size={17} />
            </button>
          </div>
          <time>
            {selectedRow.time} · ESP uptime {selectedRow.uptime_ms ?? "—"} ms
          </time>
          {selectedRow.event && (
            <p className="event-detail">
              {selectedRow.event.label} · {selectedRow.event.hex} ·{" "}
              {selectedRow.event.symbol ?? "미등록 ID"}
              {selectedRow.event.source
                ? ` · 발신 주소 ${selectedRow.event.source}`
                : ""}
              {selectedRow.event.result
                ? ` · 처리 결과 ${selectedRow.event.result}`
                : ""}
              <br />
              프로토콜 ID 해석이며, 실제 센서 상태나 최종 알림 동작을 보장하지
              않습니다.
            </p>
          )}
          <pre>{selectedRow.text}</pre>
        </section>
      )}
      <div className="log-bottom">
        <span>명령 전송과 상대 수신은 별도로 확인합니다.</span>
        <span>{rows.length.toLocaleString()}줄</span>
      </div>
    </main>
  );
}
