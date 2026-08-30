import { AbsoluteFill, Easing, interpolate, useCurrentFrame } from "remotion";
import { Rider } from "../components/Rider";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

export const Scene08Finale: React.FC = () => {
  const frame = useCurrentFrame();
  const reveal = interpolate(frame, [8, 38], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1) });
  const glow = interpolate(frame, [0, 120, 235], [0.25, 0.75, 0.38], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
  return (
    <AbsoluteFill style={{ overflow: "hidden", color: COLORS.white, background: `radial-gradient(circle at 50% 82%,rgba(255,138,76,${glow}) 0%,rgba(79,124,255,0.22) 28%,${COLORS.night} 68%)` }}>
      <div style={{ position: "absolute", inset: "110px 96px 160px", textAlign: "center", opacity: reveal }}>
        <div style={{ fontSize: 190, lineHeight: 0.88, fontWeight: 900, letterSpacing: 22, textShadow: `0 0 70px ${COLORS.cyan}44` }}>NOSTOS</div>
        <div style={{ marginTop: 34, color: COLORS.cyan, fontSize: 36, fontWeight: 800, letterSpacing: 3 }}>세 명의 라이더 · 하나의 공유 상태</div>
        <div style={{ marginTop: 46, fontSize: 58, fontWeight: 750 }}>함께 출발하고, 함께 돌아온다.</div>
        <div style={{ margin: "60px auto 0", display: "flex", width: 980, justifyContent: "space-between", alignItems: "end" }}>
          <Rider size={270} color={COLORS.cyan} label="선두" />
          <Rider size={270} color={COLORS.yellow} label="중간" />
          <Rider size={270} color={COLORS.red} label="후미" />
        </div>
      </div>
      <div style={{ position: "absolute", left: 0, right: 0, top: 48, textAlign: "center", color: COLORS.muted, fontSize: 18, letterSpacing: 3 }}>
        콘셉트 영상 · 3노드 센서 E2E 검증 진행 중
      </div>
      <Voiceover scene="scene08" from={8} />
    </AbsoluteFill>
  );
};
