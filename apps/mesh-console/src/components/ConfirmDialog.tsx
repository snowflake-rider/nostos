import { useEffect, useRef } from "react";
import type { Board } from "../types";
export function ConfirmDialog({
  board,
  onCancel,
  onConfirm,
}: {
  board: Board;
  onCancel: () => void;
  onConfirm: () => void;
}) {
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    const dialog = ref.current!;
    dialog.showModal();
    return () => dialog.close();
  }, []);
  return (
    <dialog
      ref={ref}
      className="confirm-dialog"
      aria-labelledby="confirm-title"
      onCancel={onCancel}
    >
      <h2 id="confirm-title">{board} 송신 출력을 낮출까요?</h2>
      <p>
        통신 거리가 줄어들어 Mesh 메시지가 도달하지 않을 수 있습니다. 이 보드에{" "}
        <code>tx-low</code> 명령을 한 번 보냅니다.
      </p>
      <div className="dialog-actions">
        <button autoFocus onClick={onCancel}>
          취소
        </button>
        <button className="primary" onClick={onConfirm}>
          낮은 출력 전송
        </button>
      </div>
    </dialog>
  );
}
