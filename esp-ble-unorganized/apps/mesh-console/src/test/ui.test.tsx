import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { Inspector } from "../components/Inspector";
import { BoardList } from "../components/BoardList";
import { LogWorkspace } from "../components/LogWorkspace";
import { ConfirmDialog } from "../components/ConfirmDialog";
import { filterLogs, serializeLogs, previewText } from "../logs";
import type { LogRow, NodeState } from "../types";
const node: NodeState = {
  board: "D6",
  serial: "test",
  name: "ESP32-L8-F0D6",
  detected: true,
  path: "/fake/D6",
  phase: "connected",
  error: null,
  status: {
    onoff_ready: "1",
    event_ready: "1",
    sub_C001: "1",
    primary: "0x0005",
    app: "0x0001",
  },
  status_at: Date.now(),
  fresh: true,
  last_power_request: null,
};
const rows: LogRow[] = [
  {
    id: 1,
    time: "2026-08-28T12:00:00.000Z",
    board: "D6",
    direction: "rx",
    text: "MESH_TX id=1 api=accepted",
    category: "MESH TX",
    level: "info",
    uptime_ms: 100,
  },
  {
    id: 2,
    time: "2026-08-28T12:00:01.000Z",
    board: "B6",
    direction: "rx",
    text: "failure <script>alert(1)</script>",
    category: "ERROR",
    level: "error",
    uptime_ms: 200,
  },
];
describe("USB control boundaries", () => {
  it("blocks stale, disconnected and missing C000 state", () => {
    const { rerender } = render(
      <Inspector
        node={{ ...node, fresh: false }}
        online
        busy={false}
        onCommand={vi.fn()}
      />,
    );
    expect(screen.getByRole("button", { name: "ON" })).toBeDisabled();
    rerender(
      <Inspector
        node={{ ...node, status: { ...node.status, onoff_ready: "0" } }}
        online
        busy={false}
        onCommand={vi.fn()}
      />,
    );
    expect(screen.getByRole("button", { name: "ON" })).toBeDisabled();
    expect(screen.getByRole("button", { name: "일반" })).toBeEnabled();
    rerender(
      <Inspector node={node} online={false} busy={false} onCommand={vi.fn()} />,
    );
    expect(screen.getByRole("button", { name: "상태 조회" })).toBeDisabled();
  });
  it("maps acknowledged and unacknowledged commands exactly", () => {
    const send = vi.fn();
    render(<Inspector node={node} online busy={false} onCommand={send} />);
    fireEvent.click(screen.getByRole("button", { name: "ON" }));
    expect(send).toHaveBeenLastCalledWith("on");
    fireEvent.click(screen.getByLabelText("응답 요청"));
    fireEvent.click(screen.getByRole("button", { name: "OFF" }));
    expect(send).toHaveBeenLastCalledWith("off-unack");
  });
  it("enumeration never connects until explicit click", () => {
    const toggle = vi.fn();
    render(
      <BoardList
        nodes={[{ ...node, phase: "disconnected", fresh: false, status: null }]}
        selected="D6"
        online
        busy={false}
        onToggle={toggle}
        onSelect={vi.fn()}
        onRefresh={vi.fn()}
      />,
    );
    expect(toggle).not.toHaveBeenCalled();
    fireEvent.click(screen.getByRole("button", { name: "D6 연결" }));
    expect(toggle).toHaveBeenCalledTimes(1);
  });
  it("requires an explicit confirmation for low power", () => {
    const confirm = vi.fn(),
      cancel = vi.fn();
    render(<ConfirmDialog board="D6" onConfirm={confirm} onCancel={cancel} />);
    expect(confirm).not.toHaveBeenCalled();
    fireEvent.click(screen.getByRole("button", { name: "취소" }));
    expect(cancel).toHaveBeenCalledOnce();
    fireEvent.click(screen.getByRole("button", { name: "낮은 출력 전송" }));
    expect(confirm).toHaveBeenCalledOnce();
  });
});
describe("logs", () => {
  it("combines filters and exports exact text/JSONL", () => {
    const result = filterLogs(rows, {
      board: "B6",
      category: "ERROR",
      errors: true,
      query: "FAILURE",
    });
    expect(result).toEqual([rows[1]]);
    expect(JSON.parse(serializeLogs(result, "jsonl").trim())).toEqual(rows[1]);
    expect(serializeLogs(result, "txt")).toContain(
      "[B6] [ERROR] failure <script>",
    );
    expect(previewText("I (4020) LAYER_8_MESH: MESH_TX id=1")).toBe(
      "MESH_TX id=1",
    );
  });
  it("searches, pauses display while collecting, resumes and clears only the view", () => {
    const { rerender } = render(<LogWorkspace logs={rows} instance="one" />);
    expect(document.querySelector("script")).toBeNull();
    fireEvent.change(screen.getByLabelText("로그 검색"), {
      target: { value: "MESH" },
    });
    expect(screen.getByRole("table", { name: "로그 1줄" })).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText("로그 검색"), {
      target: { value: "" },
    });
    fireEvent.click(screen.getByRole("button", { name: "일시정지" }));
    rerender(
      <LogWorkspace logs={[...rows, { ...rows[0], id: 3 }]} instance="one" />,
    );
    expect(screen.getByRole("table", { name: "로그 2줄" })).toBeInTheDocument();
    expect(screen.getByText("1줄 이후 수신")).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "재개" }));
    expect(screen.getByRole("table", { name: "로그 3줄" })).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "화면 로그 지우기" }));
    expect(screen.queryByRole("table")).toBeNull();
    expect(rows).toHaveLength(2);
  });
  it("bounds visible DOM and exposes untruncated text on row click", () => {
    const many = Array.from({ length: 5000 }, (_, i) => ({
      ...rows[0],
      id: i + 1,
    }));
    render(<LogWorkspace logs={many} instance="one" />);
    expect(document.querySelectorAll(".log-row").length).toBeLessThan(40);
    fireEvent.click(document.querySelector(".log-row")!);
    expect(screen.getByRole("region", { name: "로그 원문" })).toHaveTextContent(
      rows[0].text,
    );
  });
});
