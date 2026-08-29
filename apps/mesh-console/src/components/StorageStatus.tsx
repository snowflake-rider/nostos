import { useState } from "react";
import { request } from "../api";
import type { StorageState } from "../types";

interface Archives {
  files: { name: string; bytes: number; active: boolean }[];
  total: number;
  directory: string;
}

export function StorageStatus({ storage }: { storage?: StorageState }) {
  const [open, setOpen] = useState(false);
  const [archives, setArchives] = useState<Archives | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  async function refresh() {
    setLoading(true);
    setError(null);
    try {
      setArchives(await request<Archives>("/logs"));
    } catch (err) {
      setError(err instanceof Error ? err.message : "파일 목록 조회 실패");
    } finally {
      setLoading(false);
    }
  }
  return (
    <section className="storage-status" aria-label="로그 자동 저장">
      <div className="storage-summary">
        <span>
          {storage?.enabled
            ? `UTF-8 자동 저장 · ${storage.saved.toLocaleString()}줄 저장됨${storage.pending ? ` · ${storage.pending}줄 대기` : ""}`
            : "파일 자동 저장 꺼짐"}
        </span>
        <button
          disabled={!storage?.enabled}
          aria-expanded={open}
          onClick={() => {
            setOpen(!open);
            if (!open) void refresh();
          }}
        >
          저장 파일
        </button>
      </div>
      {storage?.error && (
        <p className="storage-error" role="alert">
          {storage.error} 미저장 {storage.missed.toLocaleString()}줄.
          폴더·디스크를 확인하고 서버를 재시작해 주세요.
        </p>
      )}
      {open && (
        <div className="archive-panel">
          <p>
            화면 필터·일시정지와 무관하게 전체 로그를 저장합니다. 파일은 자동
            삭제하지 않습니다.
          </p>
          <p className="archive-path">{storage?.directory}</p>
          <button onClick={() => void refresh()} disabled={loading}>
            {loading ? "조회 중…" : "파일 목록 새로고침"}
          </button>
          {error && <p role="alert">{error}</p>}
          {archives && (
            <>
              <p>
                최근 {archives.files.length}개 / 전체 {archives.total}개 · 실행
                중 파일은 저장 완료된 부분만 다운로드
              </p>
              <ul>
                {archives.files.map((file) => (
                  <li key={file.name}>
                    <a
                      href={`/api/logs/${encodeURIComponent(file.name)}`}
                      download
                    >
                      {file.active ? "현재 세션 · " : ""}
                      {file.name}
                    </a>
                    <span>{(file.bytes / 1024).toFixed(1)} KiB</span>
                  </li>
                ))}
              </ul>
            </>
          )}
        </div>
      )}
    </section>
  );
}
