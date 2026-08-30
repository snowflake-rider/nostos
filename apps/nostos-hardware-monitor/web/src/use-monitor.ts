import { useCallback, useEffect, useState } from "react";

import type { MonitorControl, WebMonitorState } from "../../src/web-contract";
import { fetchMonitorState, sendControl, subscribeMonitorState } from "./api";

export function useMonitor() {
  const [state, setState] = useState<WebMonitorState>();
  const [streamConnected, setStreamConnected] = useState(false);
  const [error, setError] = useState<string>();
  const [controlPending, setControlPending] = useState(false);

  useEffect(() => {
    let active = true;
    void fetchMonitorState()
      .then((next) => {
        if (active) setState(next);
      })
      .catch((reason: unknown) => {
        if (active) setError(reason instanceof Error ? reason.message : String(reason));
      });
    const unsubscribe = subscribeMonitorState(
      (next) => {
        if (!active) return;
        setState(next);
        setError(undefined);
      },
      (connected) => {
        if (active) setStreamConnected(connected);
      },
    );
    return () => {
      active = false;
      unsubscribe();
    };
  }, []);

  const control = useCallback(async (command: MonitorControl) => {
    setControlPending(true);
    try {
      const next = await sendControl(command);
      setState(next);
      setError(undefined);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : String(reason));
    } finally {
      setControlPending(false);
    }
  }, []);

  return { state, streamConnected, error, controlPending, control };
}
