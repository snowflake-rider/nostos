import { Audio } from "@remotion/media";
import { Easing, interpolate, staticFile, useCurrentFrame } from "remotion";
import { Rider, SignalPulse } from "../components/Rider";
import { Eyebrow, Headline, SceneShell } from "../components/SceneShell";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

export const Scene06Safety: React.FC = () => {
  const frame = useCurrentFrame();
  const incident = interpolate(frame, [65, 120], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1) });
  const pulse = frame < 70 ? 0 : ((frame - 70) % 72) / 72;
  return (
    <SceneShell label="06 / 긴급 상황 우선 설계" accent={COLORS.red}>
      <div style={{ position: "absolute", left: 0, top: 45, width: 760 }}>
        <Eyebrow color={COLORS.red}>매 순간이 중요할 때</Eyebrow>
        <Headline size={82}>경고가 그룹의 공유 상태가 됩니다.</Headline>
        <div style={{ marginTop: 38, display: "flex", gap: 16 }}>
          {["낙차", "SOS", "정지"].map((type) => <div key={type} style={{ padding: "14px 22px", borderRadius: 999, border: `1px solid ${COLORS.red}66`, color: COLORS.red, fontSize: 25, fontWeight: 850 }}>{type}</div>)}
        </div>
      </div>
      <div style={{ position: "absolute", right: -30, top: 20, width: 880, height: 640, borderRadius: 44, overflow: "hidden", background: "radial-gradient(circle at 70% 65%,rgba(255,71,111,0.25),transparent 34%),linear-gradient(180deg,#111B32,#050914)", border: `1px solid ${COLORS.red}55` }}>
        <div style={{ position: "absolute", left: 55, top: 95 }}><Rider size={250} color={COLORS.cyan} label="선두" /></div>
        <div style={{ position: "absolute", left: 285, top: 245 }}><Rider size={230} color={COLORS.yellow} label="중간" /></div>
        <div style={{ position: "absolute", left: 520, top: 400, rotate: `${incident * 18}deg` }}>
          <div style={{ position: "relative" }}>
            <Rider size={220} color={COLORS.red} label="후미" tilt={incident * 24} />
            <SignalPulse color={COLORS.red} progress={pulse} size={440} />
          </div>
        </div>
        <div style={{ position: "absolute", right: 42, top: 42, padding: "16px 23px", borderRadius: 20, background: `${COLORS.red}1F`, border: `1px solid ${COLORS.red}88`, color: COLORS.red, fontSize: 25, fontWeight: 900, letterSpacing: 2, opacity: incident }}>긴급 / 낙차</div>
      </div>
      <Audio from={112} src={staticFile("sfx/rear_warning.mp3")} volume={0.22} />
      <Voiceover scene="scene06" from={8} />
    </SceneShell>
  );
};
