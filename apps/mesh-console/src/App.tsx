import { useState } from "react";
import { Cable, ChevronRight, X } from "lucide-react";
import { BoardList } from "./components/BoardList";
import { Inspector } from "./components/Inspector";
import { LogWorkspace } from "./components/LogWorkspace";
import { ConfirmDialog } from "./components/ConfirmDialog";
import { request } from "./api";
import { isOpen, type Board, type Command, type NodeState } from "./types";
import { useBridge } from "./useBridge";
export default function App() {
  const { state, logs, online, error } = useBridge();
  const [selected, setSelected] = useState<Board>("D6");
  const [busy, setBusy] = useState(false);
  const [notice, setNotice] = useState<{ text: string; error: boolean } | null>(
    null,
  );
  const [confirmBoard, setConfirmBoard] = useState<Board | null>(null);
  const [panel, setPanel] = useState<"logs" | "boards" | "controls">("logs");
  const nodes = state?.nodes ?? [];
  const node = nodes.find((n) => n.board === selected);
  async function action(task: () => Promise<unknown>, message: string) {
    if (busy) return;
    setBusy(true);
    setNotice(null);
    try {
      await task();
      setNotice({ text: message, error: false });
    } catch (err) {
      setNotice({
        text:
          err instanceof Error ? err.message : "요청을 완료하지 못했습니다.",
        error: true,
      });
    } finally {
      setBusy(false);
    }
  }
  function toggle(n: NodeState) {
    void action(
      () =>
        request(
          `/boards/${n.board}/${isOpen(n) ? "disconnect" : "connect"}`,
          {},
        ),
      `${n.board} ${isOpen(n) ? "연결 해제됨" : "USB 연결 요청 완료 · 콘솔 확인 중"}`,
    );
  }
  function send(command: Command, board = selected, confirmed = false) {
    if (command === "tx-low" && !confirmed) {
      setConfirmBoard(board);
      return;
    }
    void action(
      () =>
        request(`/boards/${board}/command`, {
          command,
          confirm_low_power: confirmed,
        }),
      `${board} · ${command} USB에 전달됨. 실행·상대 수신은 로그를 확인하세요.`,
    );
  }
  return (
    <div className={`app panel-${panel}`}>
      <header className="app-header">
        <a className="brand" href="/">
          Mesh Console
        </a>
        <span className="header-subtitle">USB 연결 · Layer 8</span>
        <span className="header-status">
          <span className={`dot ${online ? "green" : ""}`} />
          {state?.mode === "test"
            ? "테스트 모드 · 가상 USB"
            : online
              ? "로컬 서버 연결됨"
              : "로컬 서버 확인 중"}
        </span>
      </header>
      <nav className="mobile-nav" aria-label="화면 선택">
        <button
          aria-pressed={panel === "boards"}
          onClick={() => setPanel(panel === "boards" ? "logs" : "boards")}
        >
          보드
        </button>
        <button
          aria-pressed={panel === "logs"}
          onClick={() => setPanel("logs")}
        >
          메시지 로그
        </button>
        <button
          aria-pressed={panel === "controls"}
          onClick={() => setPanel(panel === "controls" ? "logs" : "controls")}
        >
          {selected} 제어
        </button>
      </nav>
      {(error || state?.scan_error || notice) && (
        <div
          className={`notice ${error || state?.scan_error || notice?.error ? "notice-error" : ""}`}
          role="status"
        >
          <span>{error || state?.scan_error || notice?.text}</span>
          {!error && !state?.scan_error && (
            <button
              aria-label="알림 닫기"
              className="icon-button"
              onClick={() => setNotice(null)}
            >
              <X size={16} />
            </button>
          )}
        </div>
      )}
      <div className="app-body">
        <BoardList
          nodes={nodes}
          selected={selected}
          online={online}
          busy={busy}
          onSelect={setSelected}
          onToggle={toggle}
          onRefresh={() =>
            void action(
              () => request("/state"),
              "USB 목록을 새로 확인했습니다. 포트는 열지 않았습니다.",
            )
          }
        />
        <LogWorkspace
          key={state?.instance ?? "waiting"}
          logs={logs}
          instance={state?.instance ?? ""}
          storage={state?.storage}
        />
        <Inspector
          key={selected}
          node={node}
          online={online}
          busy={busy}
          onCommand={send}
        />
      </div>
      <footer className="app-footer">
        <span className="footer-brand">
          <Cable size={18} />
          LOCAL USB BRIDGE
        </span>
        <span className="footer-state">
          <span className={`dot ${online ? "green" : ""}`} />
          {online
            ? `서버 연결됨 · USB ${nodes.filter(isOpen).length}/3`
            : "서버 연결 대기"}
        </span>
        <span className="footer-retained">
          {logs.length.toLocaleString()}줄 보관
          {state?.dropped
            ? ` · 화면에서 이전 ${state.dropped.toLocaleString()}줄 생략`
            : ""}
        </span>
        <span className="footer-manual">
          포트 자동 연결 꺼짐
          <ChevronRight size={15} />
        </span>
      </footer>
      {confirmBoard && (
        <ConfirmDialog
          board={confirmBoard}
          onCancel={() => setConfirmBoard(null)}
          onConfirm={() => {
            send("tx-low", confirmBoard, true);
            setConfirmBoard(null);
          }}
        />
      )}
    </div>
  );
}
