import type { Board, LogRow } from "./types";
export interface Filters {
  board: Board | "all";
  category: string;
  query: string;
  errors: boolean;
}
export function filterLogs(logs: LogRow[], f: Filters) {
  const query = f.query.trim().toLocaleLowerCase();
  return logs.filter(
    (row) =>
      (f.board === "all" || row.board === f.board) &&
      (f.category === "all" || row.category === f.category) &&
      (!f.errors || row.level === "error") &&
      (!query ||
        `${row.text} ${row.board} ${row.category} ${row.event?.label ?? ""} ${row.event?.symbol ?? ""}`
          .toLocaleLowerCase()
          .includes(query)),
  );
}
export function formatTime(value: string) {
  const date = new Date(value);
  return (
    date.toLocaleTimeString("en-GB", { hour12: false }) +
    "." +
    String(date.getMilliseconds()).padStart(3, "0")
  );
}
export function previewText(text: string) {
  return text.replace(/^[IWEVD] \(\d+\) [^:]+: /, "");
}
export function serializeLogs(rows: LogRow[], format: "txt" | "jsonl") {
  return (
    rows
      .map((row) =>
        format === "jsonl"
          ? JSON.stringify(row)
          : `${row.time} [${row.board}] [${row.category}] ${row.event ? `[${row.event.label} ${row.event.hex}] ` : ""}${row.text}`,
      )
      .join("\n") + (rows.length ? "\n" : "")
  );
}
export function downloadLogs(rows: LogRow[], format: "txt" | "jsonl") {
  const url = URL.createObjectURL(
    new Blob([serializeLogs(rows, format)], {
      type: "text/plain;charset=utf-8",
    }),
  );
  const link = document.createElement("a");
  link.href = url;
  link.download = `mesh-logs-${new Date().toISOString().replace(/[:.]/g, "-")}.${format}`;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
