import { Easing, interpolate, useCurrentFrame } from "remotion";
import { Eyebrow, Headline, SceneShell } from "../components/SceneShell";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

const FlowNode: React.FC<{ title: string; detail: string; color: string; icon: string }> = ({ title, detail, color, icon }) => (
  <div style={{ width: 270, minHeight: 210, padding: "28px 24px", borderRadius: 30, background: "rgba(11,18,36,0.94)", border: `1px solid ${color}55`, textAlign: "center", boxShadow: `0 18px 55px ${color}14` }}>
    <div style={{ width: 64, height: 64, margin: "0 auto 18px", borderRadius: 20, display: "grid", placeItems: "center", background: `${color}1D`, color, fontSize: 33, fontWeight: 900 }}>{icon}</div>
    <div style={{ fontSize: 30, fontWeight: 850 }}>{title}</div>
    <div style={{ marginTop: 8, color: COLORS.muted, fontSize: 21, lineHeight: 1.3 }}>{detail}</div>
  </div>
);

export const Scene04Architecture: React.FC = () => {
  const frame = useCurrentFrame();
  const packet = interpolate(frame % 135, [0, 134], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.linear });
  const flowWidth = 1420;
  return (
    <SceneShell label="04 / 모든 노드의 내부">
      <Eyebrow>의도에서 공유 인식까지</Eyebrow>
      <Headline size={78}>하나의 신호 경로를 한눈에.</Headline>
      <div style={{ position: "relative", marginTop: 72, height: 260 }}>
        <div style={{ position: "absolute", left: 115, top: 108, width: flowWidth, height: 4, background: "rgba(53,230,255,0.18)" }} />
        <div style={{ position: "absolute", left: 115 + packet * flowWidth, top: 96, width: 28, height: 28, borderRadius: "50%", background: COLORS.cyan, boxShadow: `0 0 34px ${COLORS.cyan}` }} />
        <div style={{ position: "absolute", left: 0, top: 0 }}><FlowNode title="입력" detail="센서 + 버튼" color={COLORS.green} icon="IN" /></div>
        <div style={{ position: "absolute", left: 350, top: 0 }}><FlowNode title="STM32" detail="판단 + 처리" color={COLORS.cyan} icon="µC" /></div>
        <div style={{ position: "absolute", left: 700, top: 0 }}><FlowNode title="UART" detail="로컬 브리지" color={COLORS.blue} icon="⇄" /></div>
        <div style={{ position: "absolute", left: 1050, top: 0 }}><FlowNode title="ESP32-S3" detail="Mesh 전송" color={COLORS.yellow} icon="RF" /></div>
        <div style={{ position: "absolute", left: 1400, top: 0 }}><FlowNode title="그룹" detail="3개 공유 노드" color={COLORS.red} icon="3×" /></div>
      </div>
      <div style={{ display: "flex", justifyContent: "center", gap: 30, marginTop: 42 }}>
        {[{ t: "OLED", c: COLORS.cyan }, { t: "RGB", c: COLORS.green }, { t: "오디오", c: COLORS.yellow }, { t: "부저", c: COLORS.red }].map((output) => (
          <div key={output.t} style={{ padding: "17px 30px", minWidth: 170, textAlign: "center", borderRadius: 999, border: `1px solid ${output.c}55`, background: `${output.c}10`, color: output.c, fontSize: 23, fontWeight: 850, letterSpacing: 2 }}>{output.t}</div>
        ))}
      </div>
      <Voiceover scene="scene04" from={8} />
    </SceneShell>
  );
};
