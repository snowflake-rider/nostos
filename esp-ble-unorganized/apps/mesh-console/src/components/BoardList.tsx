import { RefreshCw, Usb } from "lucide-react";
import type { Board, NodeState } from "../types";
import { isOpen, phaseLabel } from "../types";
interface Props {
  nodes: NodeState[];
  selected: Board;
  online: boolean;
  busy: boolean;
  onSelect: (board: Board) => void;
  onRefresh: () => void;
  onToggle: (node: NodeState) => void;
}
export function BoardList({
  nodes,
  selected,
  online,
  busy,
  onSelect,
  onRefresh,
  onToggle,
}: Props) {
  return (
    <aside className="board-panel" aria-label="보드 목록">
      <div className="panel-heading">
        <h2>보드</h2>
        <button
          className="icon-button"
          aria-label="USB 목록 새로고침"
          title="포트를 열지 않고 목록만 새로고침"
          onClick={onRefresh}
          disabled={!online || busy}
        >
          <RefreshCw size={18} />
        </button>
      </div>
      <div className="board-list">
        {nodes.map((node) => (
          <div
            key={node.board}
            className={`board-row ${selected === node.board ? "selected" : ""}`}
          >
            <button
              className="board-select"
              onClick={() => onSelect(node.board)}
              aria-pressed={selected === node.board}
              aria-label={`${node.board} 보드 선택`}
            >
              <span className="board-title">
                <span
                  className={`dot ${online && node.fresh ? "green" : node.phase === "error" ? "red" : ""}`}
                />
                <strong>{node.board}</strong>
              </span>
              <span className="board-phase">{phaseLabel(node, online)}</span>
              <span className="serial">{node.serial}</span>
            </button>
            <button
              className="connect-button"
              aria-label={`${node.board} ${isOpen(node) ? "연결 해제" : "연결"}`}
              onClick={() => onToggle(node)}
              disabled={!online || busy || (!node.detected && !isOpen(node))}
            >
              {isOpen(node) ? "해제" : "연결"}
            </button>
          </div>
        ))}
      </div>
      {!nodes.length && (
        <p className="rail-loading">로컬 서버를 확인하고 있습니다.</p>
      )}
      <div className="rail-note">
        <Usb size={18} />
        <p>
          연결 버튼을 누를 때만
          <br />
          USB 포트를 엽니다.
        </p>
      </div>
    </aside>
  );
}
