import { BoxRenderable, ScrollBoxRenderable, TextAttributes, TextRenderable,
  type CliRenderer, type KeyEvent } from "@opentui/core";
import { ageLabel, BOARDS, readiness, type Report } from "./report";
import type { Scanner, ScanTask } from "./scanner";

const C = { bg: "#101722", panel: "#162131", text: "#dce6f1", muted: "#9eacc0",
  accent: "#62dac6", warning: "#f1c479", border: "#34465e", selected: "#243951" };

export function createDashboard(renderer: CliRenderer, scanner: Scanner, demo = false) {
  const state = { selected: 0, help: false, loading: false, report: null as Report | null,
    failed: false, disposed: false };
  let task: ScanTask | null = null;
  const root = new BoxRenderable(renderer, { id: "dashboard", width: "100%", height: "100%",
    flexDirection: "column", backgroundColor: C.bg, paddingX: 1 });
  const header = new TextRenderable(renderer, { id: "header", height: 2, flexShrink: 0,
    fg: C.accent, attributes: TextAttributes.BOLD, wrapMode: "word", content: "" });
  const notice = new TextRenderable(renderer, { id: "notice", height: 2, flexShrink: 0,
    fg: C.warning, wrapMode: "word", content: "" });
  const body = new BoxRenderable(renderer, { id: "body", flexGrow: 1, minHeight: 1, flexDirection: "row", gap: 1 });
  const sidebar = new BoxRenderable(renderer, { id: "boards", width: 24, flexShrink: 0, flexDirection: "column", gap: 1 });
  const cards = BOARDS.map((board, i) => {
    const card = new BoxRenderable(renderer, { id: `board-${board}`, height: 4, flexShrink: 0, border: true,
      borderStyle: "rounded", paddingX: 1, title: `${i + 1} · ${board}`, backgroundColor: C.panel });
    const label = new TextRenderable(renderer, { id: `label-${board}`, content: "조회 대기", fg: C.text, wrapMode: "word" });
    card.add(label); sidebar.add(card);
    return { card, label };
  });
  const sidebarNote = new TextRenderable(renderer, { id: "sidebar-note", content: "같은 조회 엔진\nesp32-scan --json\n\n실시간 연결 표시 아님", fg: C.muted, wrapMode: "word" });
  sidebar.add(sidebarNote);
  const detail = new ScrollBoxRenderable(renderer, { id: "detail", flexGrow: 1, minWidth: 1,
    border: true, borderColor: C.border, title: "보드 설정 · 관찰값", scrollX: false, scrollY: true,
    contentOptions: { flexDirection: "column", paddingX: 1, paddingY: 1 }, backgroundColor: C.panel });
  const detailText = new TextRenderable(renderer, { id: "detail-text", content: "", width: "100%",
    fg: C.text, wrapMode: "word", flexShrink: 0 });
  detail.add(detailText);
  body.add(sidebar); body.add(detail);
  const footer = new TextRenderable(renderer, { id: "footer", height: 2, flexShrink: 0,
    content: "", fg: C.muted, wrapMode: "word" });
  root.add(header); root.add(notice); root.add(body); root.add(footer); renderer.root.add(root);

  const render = () => {
    if (state.disposed) return;
    const small = renderer.width < 76;
    sidebar.visible = !small && renderer.height >= 22;
    header.content = `NOSTOS / ESP32   ${demo ? "DEMO · 가상 데이터" : "READ ONLY · 설정 조회"}\n` +
      (small ? BOARDS.map((b, i) => `${state.selected === i ? "[" : " "}${i + 1}:${b}${state.selected === i ? "]" : " "}`).join(" ") : ageLabel(state.report));
    notice.content = state.loading ? "내부 통신 준비·조회 중…  추가 조회는 대기 / q 종료 가능" : state.failed
      ? "조회 실패 · 아래 값이 있으면 이전 관찰입니다. 오류 확인 후 r로 다시 조회하세요."
      : demo ? "데모 화면입니다. USB·Console에 접속하지 않습니다."
      : state.report?.result === "INCOMPLETE" ? "일부 보드를 읽지 못했습니다. 아래 조회 결과를 확인하세요."
      : "설정 요약 관찰용 · 전체 설정/무선 성공 판정 아님 · r로 새로 조회";
    cards.forEach(({ card, label }, i) => {
      card.borderColor = i === state.selected ? C.accent : C.border;
      card.backgroundColor = i === state.selected ? C.selected : C.panel;
      const d = state.report?.devices[i];
      label.content = `${d?.status?.primary ?? "주소 미확인"}\n${d?.result === "READ" ? "요약 읽음" : d?.path ? "USB 감지 / 미조회" : "미감지 / 대기"}`;
    });
    const d = state.report?.devices[state.selected];
    const s = d?.status;
    detail.title = state.help ? "OnOff 설정 안내 · 화면에서 변경하지 않음" : `${BOARDS[state.selected]} · 설정 요약`;
    const value = (key: keyof NonNullable<typeof s>) => s?.[key] ?? "조회 불가";
    detailText.content = state.help ? [
      "세 보드 모두 같은 방법입니다.", "", "1. nRF Mesh → 노드 → Element → Generic OnOff Client",
      "   COMMON-ONOFF Bind", "   Publication C000 / 같은 AppKey / TTL 7 / Period 0", "",
      "2. Generic OnOff Server", "   같은 COMMON-ONOFF Bind", "   Subscription C000 추가", "",
      "기존 Vendor C001 설정과 Relay 설정은 유지합니다.",
      "Client=송신 기능, Server=수신 기능입니다.", "",
      "앱에서 재조회한 설정과 onoff_ready가 다르면 반복 설정하지 마세요.",
      "현재 펌웨어의 상태 갱신/실제 설정을 따로 확인해야 합니다.", "",
      "r: USB 요약 다시 읽기   h: 설정 요약으로 돌아가기",
      "시험 실행은 tests/mesh/README.md를 따릅니다.",
    ].join("\n") : [
      `${demo ? "DEMO / " : ""}${ageLabel(state.report)}`,
      `조회 결과  ${d?.result ?? "NOT_READ"}`,
      `STATUS 수신 (UTC)  ${d?.receivedAt ?? "미수신"}`,
      `USB        ${d?.path ?? "조회 전 / 미감지"}`,
      `장치       ${value("name")}`,
      `주소       ${value("primary")}`, "",
      "C001 · Vendor 이벤트 모델",
      `발행 ${value("pub")}   구독 ${value("sub_C001")}`,
      `NetIdx ${value("net")}   AppIdx ${value("app")}`,
      `TTL ${value("ttl")}   Period ${value("period")}   발행 재전송 ${value("retransmit")}`,
      `이벤트 준비: ${readiness(s?.event_ready)}`, "",
      "C000 · Generic OnOff",
      `송신 Client: ${readiness(s?.onoff_ready)}`,
      `OnOff 상태: ${value("state")}`,
      "수신 Server의 Bind/구독: 조회 불가", "",
      `Relay 캐시 보고: ${value("relay_cached")} · 현재 설정 확정 불가`, "",
      "읽을 수 없는 항목",
      "• 모델별 실제 Bind 목록 / C000 발행 세부값",
      "• 현재 Relay 및 Relay 재전송 / 내부 설정 갱신 시각", "",
      "새 STATUS 수신 ≠ 내부 Mesh 설정의 최신 재조회.",
      "OnOff 0은 송신 준비의 보고값이지 수신 실패가 아닙니다.",
      "상태 조회 후 연결은 Console의 기존 정책을 따릅니다.",
    ].join("\n");
    footer.content = renderer.width < 50 ? "1/2/3 보드  r 조회  h 안내\n↑↓ 스크롤  q 종료" :
      "1/2/3 보드 선택   r 새로고침   h 설정 안내   ↑↓ / PgUp·Dn 스크롤\nq / Esc 종료   설정·Relay·ON/OFF 변경 없음";
    renderer.requestRender();
  };
  const refresh = async () => {
    if (state.loading || state.disposed) return;
    state.loading = true; state.failed = false; render();
    try {
      task = scanner();
      const report = await task.promise;
      if (!state.disposed) state.report = report;
    } catch {
      if (!state.disposed) state.failed = true;
    } finally {
      task = null; state.loading = false; render();
    }
  };
  const keypress = (key: KeyEvent) => {
    if (key.name === "q" || key.name === "escape" || (key.ctrl && key.name === "c")) { renderer.destroy(); return; }
    if (key.ctrl || key.meta || key.option) return;
    if (["1", "2", "3"].includes(key.name)) { state.selected = Number(key.name) - 1; detail.scrollTo(0); }
    else if (key.name === "r") { void refresh(); return; }
    else if (key.name === "h") { state.help = !state.help; detail.scrollTo(0); }
    else if (key.name === "down" || key.name === "j") detail.scrollBy(1);
    else if (key.name === "up" || key.name === "k") detail.scrollBy(-1);
    else if (key.name === "pagedown") detail.scrollBy(8);
    else if (key.name === "pageup") detail.scrollBy(-8);
    render();
  };
  const dispose = () => {
    if (state.disposed) return;
    state.disposed = true; task?.cancel(); clearInterval(timer);
    renderer.keyInput.off("keypress", keypress); renderer.off("resize", render);
  };
  const timer = setInterval(render, 1000); // Age label only; no periodic USB/HTTP operations.
  renderer.keyInput.on("keypress", keypress);
  renderer.on("resize", render);
  renderer.on("destroy", dispose);
  render();
  return { state, refresh, dispose };
}
