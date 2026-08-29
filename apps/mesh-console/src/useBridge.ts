import { useEffect, useState } from "react";
import type { BridgeState, LogRow, WireMessage } from "./types";

export function useBridge() {
  const [state, setState] = useState<BridgeState | null>(null);
  const [logs, setLogs] = useState<LogRow[]>([]);
  const [online, setOnline] = useState(false);
  const [error, setError] = useState<string | null>(null);
  useEffect(() => {
    let stopped = false;
    let socket: WebSocket | null = null;
    let retry: ReturnType<typeof setTimeout>;
    let delay = 500;
    let pending: LogRow[] = [];
    let instance = "";
    const flush = setInterval(() => {
      if (pending.length) {
        const batch = pending;
        pending = [];
        setLogs((old) => [...old, ...batch].slice(-5000));
      }
    }, 100);
    function connect() {
      if (stopped) return;
      socket = new WebSocket(
        `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/api/stream`,
      );
      socket.onmessage = (event) => {
        if (stopped) return;
        try {
          const message = JSON.parse(event.data) as WireMessage;
          if (message.type === "snapshot") {
            pending = [];
            instance = message.state.instance;
            setState(message.state);
            setLogs(message.logs.slice(-5000));
            setOnline(true);
            setError(null);
            delay = 500;
          } else if (
            message.type === "state" &&
            message.instance === instance
          ) {
            setState(message);
          } else if (message.type === "log") {
            pending.push(message.log);
            if (pending.length > 5000) pending = pending.slice(-5000);
          }
        } catch {
          setError("로그 스트림 형식을 읽지 못했습니다.");
          socket?.close();
        }
      };
      socket.onclose = () => {
        if (stopped) return;
        setOnline(false);
        setError("로컬 서버 연결이 끊겼습니다. 다시 연결하는 중입니다.");
        retry = setTimeout(connect, delay);
        delay = Math.min(delay * 2, 5000);
      };
      socket.onerror = () => socket?.close();
    }
    connect();
    return () => {
      stopped = true;
      clearInterval(flush);
      clearTimeout(retry);
      socket?.close();
    };
  }, []);
  return { state, logs, online, error };
}
