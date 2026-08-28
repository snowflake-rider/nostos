import { Check, Info, RefreshCw } from "lucide-react";
import { useState } from "react";
import type { Command, NodeState } from "../types";
import { isOpen, phaseLabel } from "../types";
interface Props {
  node?: NodeState;
  online: boolean;
  busy: boolean;
  onCommand: (command: Command) => void;
}
export function Inspector({ node, online, busy, onCommand }: Props) {
  const [ack, setAck] = useState(true);
  const fresh = !!(online && node?.fresh);
  const status = node?.status;
  const value = (key: string) => status?.[key] ?? "—";
  const ready = fresh && status?.onoff_ready === "1";
  return (
    <aside className="inspector" aria-label="선택한 보드 제어">
      <div className="inspector-top">
        <div className="inspector-title">
          <h2>{node?.board ?? "D6"}</h2>
          <span>선택한 보드</span>
        </div>
        <p className="connection-line">
          <span className={`dot ${fresh ? "green" : ""}`} />
          {node ? phaseLabel(node, online) : "서버 확인 중"}
        </p>
        <button
          className="primary"
          disabled={!online || !node || !isOpen(node) || busy}
          onClick={() => onCommand("status")}
        >
          <RefreshCw size={18} />
          상태 조회
        </button>
        <p className="poll-note">
          연결 중 5초마다 갱신
          {node?.status_at
            ? ` · ${new Date(node.status_at).toLocaleTimeString("ko-KR", { hour12: false })}`
            : ""}
        </p>
      </div>
      {node?.error && (
        <p className="node-error" role="status">
          {node.error}
        </p>
      )}
      <section
        className={`inspector-section ${status && !fresh ? "stale" : ""}`}
      >
        <h3>Mesh 상태</h3>
        <dl>
          <div>
            <dt>주소</dt>
            <dd className="mono">{value("primary")}</dd>
          </div>
          <div>
            <dt>이벤트 송신</dt>
            <dd className={fresh && status?.event_ready === "1" ? "good" : ""}>
              {status ? (
                status.event_ready === "1" ? (
                  <>
                    <Check size={15} />
                    준비됨
                  </>
                ) : (
                  "미준비"
                )
              ) : (
                "—"
              )}
            </dd>
          </div>
          <div>
            <dt>C001 구독</dt>
            <dd className={fresh && status?.sub_C001 === "1" ? "good" : ""}>
              {status ? (
                status.sub_C001 === "1" ? (
                  <>
                    <Check size={15} />
                    설정됨
                  </>
                ) : (
                  "미설정"
                )
              ) : (
                "—"
              )}
            </dd>
          </div>
          <div>
            <dt>AppKey</dt>
            <dd className="mono">{value("app")}</dd>
          </div>
        </dl>
        {status && !fresh && (
          <p className="warning">오래된 상태 · 제어하려면 다시 조회하세요.</p>
        )}
      </section>
      <section className="inspector-section">
        <h3>
          On/Off 시험 <span>· C000</span>
        </h3>
        <p className="helper">C001 버튼 이벤트와 별개입니다.</p>
        <div className="button-pair">
          <button
            disabled={!ready || busy}
            onClick={() => onCommand(ack ? "on" : "on-unack")}
          >
            ON
          </button>
          <button
            disabled={!ready || busy}
            onClick={() => onCommand(ack ? "off" : "off-unack")}
          >
            OFF
          </button>
        </div>
        <label className="check-label">
          <input
            type="checkbox"
            checked={ack}
            onChange={(e) => setAck(e.target.checked)}
          />
          응답 요청
        </label>
        {fresh && !ready && (
          <p className="warning">
            C000 모델 미준비 · nRF Mesh 설정을 확인하세요.
          </p>
        )}
      </section>
      <section className="inspector-section">
        <h3>송신 출력</h3>
        <div className="button-pair">
          <button
            disabled={!fresh || busy}
            onClick={() => onCommand("tx-normal")}
          >
            일반
          </button>
          <button disabled={!fresh || busy} onClick={() => onCommand("tx-low")}>
            낮음
          </button>
        </div>
        <p className="helper info-line">
          <Info size={15} />
          낮은 출력은 통신 거리를 줄입니다.
        </p>
        {node?.last_power_request && (
          <p className="helper">
            마지막 요청:{" "}
            {node.last_power_request === "tx-low" ? "낮음" : "일반"} · 현재 출력
            확인 아님
          </p>
        )}
      </section>
      <div className="inspector-note">
        Mesh 등록과 키 설정은 <strong>nRF Mesh</strong>에서 진행합니다.
      </div>
    </aside>
  );
}
