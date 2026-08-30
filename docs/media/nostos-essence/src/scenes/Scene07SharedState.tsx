import { Easing, interpolate, useCurrentFrame } from "remotion";
import { Eyebrow, Headline, SceneShell } from "../components/SceneShell";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

const rows = [
  { role: "A / 선두", value: "속도", detail: "XOSS", color: COLORS.cyan, glyph: "27.4" },
  { role: "B / 중간", value: "환경", detail: "DHT11", color: COLORS.yellow, glyph: "24°" },
  { role: "C / 후미", value: "안전", detail: "MPU6050", color: COLORS.red, glyph: "정상" },
] as const;

export const Scene07SharedState: React.FC = () => {
  const frame = useCurrentFrame();
  const shared = interpolate(frame, [32, 125], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1) });
  return (
    <SceneShell label="07 / 한눈에 보는 그룹" accent={COLORS.blue}>
      <Eyebrow color={COLORS.blue}>공유 대시보드</Eyebrow>
      <Headline size={82}>핸들 너머의 상황까지 봅니다.</Headline>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1.08fr", gap: 60, marginTop: 54, height: 420 }}>
        <div style={{ display: "flex", flexDirection: "column", gap: 16 }}>
          {rows.map((row, index) => (
            <div key={row.role} style={{ flex: 1, display: "grid", gridTemplateColumns: "160px 1fr 96px", alignItems: "center", padding: "0 26px", borderRadius: 24, border: `1px solid ${row.color}44`, background: "rgba(11,18,36,0.78)", opacity: interpolate(shared, [index * 0.2, index * 0.2 + 0.38], [0.2, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" }) }}>
              <div style={{ color: row.color, fontSize: 21, fontWeight: 850, letterSpacing: 2 }}>{row.role}</div>
              <div><div style={{ fontSize: 31, fontWeight: 850 }}>{row.value}</div><div style={{ color: COLORS.muted, fontSize: 19 }}>{row.detail}</div></div>
              <div style={{ color: row.color, fontSize: 29, fontWeight: 900, textAlign: "right" }}>{row.glyph}</div>
            </div>
          ))}
        </div>
        <div style={{ borderRadius: 34, padding: "34px", border: "1px solid rgba(255,255,255,0.15)", background: "linear-gradient(155deg,rgba(17,27,50,0.96),rgba(7,13,28,0.96))", boxShadow: "0 28px 90px rgba(0,0,0,0.35)" }}>
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 21, color: COLORS.muted, letterSpacing: 2 }}><span>NOSTOS 센서</span><span>3개 노드</span></div>
          <div style={{ marginTop: 38, fontSize: 48, fontWeight: 900 }}>그룹 상태</div>
          <div style={{ marginTop: 32, display: "grid", gridTemplateColumns: "repeat(3,1fr)", gap: 18 }}>
            {rows.map((row) => (
              <div key={row.role} style={{ height: 190, borderRadius: 24, padding: "24px 18px", background: `${row.color}12`, border: `1px solid ${row.color}44` }}>
                <div style={{ color: row.color, fontSize: 20, fontWeight: 850 }}>{row.role.split(" / ")[0]}</div>
                <div style={{ marginTop: 24, fontSize: 44, fontWeight: 900 }}>{row.glyph}</div>
                <div style={{ marginTop: 8, color: COLORS.muted, fontSize: 18 }}>{row.value}</div>
              </div>
            ))}
          </div>
          <div style={{ marginTop: 22, color: COLORS.green, fontSize: 22, fontWeight: 800 }}>● 모든 노드 확인됨</div>
        </div>
      </div>
      <div style={{ position: "absolute", right: 0, top: 40, padding: "10px 18px", borderRadius: 999, border: `1px solid ${COLORS.yellow}55`, color: COLORS.yellow, fontSize: 19, fontWeight: 800, letterSpacing: 2 }}>목표 대시보드</div>
      <Voiceover scene="scene07" from={8} />
    </SceneShell>
  );
};
