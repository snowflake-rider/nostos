import { Easing, interpolate, useCurrentFrame } from "remotion";
import { Headline, SceneShell } from "../components/SceneShell";
import { Rider } from "../components/Rider";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

export const Scene01Hook: React.FC = () => {
  const frame = useCurrentFrame();
  const roadShift = (frame * 14) % 180;
  const shadow = interpolate(frame, [55, 150], [0.1, 0.88], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
    easing: Easing.bezier(0.16, 1, 0.3, 1),
  });

  return (
    <SceneShell label="01 / 시야가 닿지 않는 순간" accent={COLORS.red}>
      <div style={{ position: "absolute", left: 0, top: 90, width: 840 }}>
        <Headline size={92}>위험은 시야가 닿지 않는 곳에서 시작됩니다.</Headline>
        <div style={{ marginTop: 34, color: COLORS.muted, fontSize: 32, lineHeight: 1.35, width: 680 }}>
          세 명의 라이더. 서로 다른 세 개의 시점. 하나의 인지 공백.
        </div>
      </div>

      <div style={{ position: "absolute", right: -90, top: -6, width: 880, height: 710, overflow: "hidden", borderRadius: 48, border: "1px solid rgba(255,255,255,0.12)", background: "linear-gradient(180deg,#111B32 0%,#050914 85%)" }}>
        <div style={{ position: "absolute", inset: 0, background: `linear-gradient(90deg, transparent 36%, rgba(5,9,20,${shadow}) 88%)` }} />
        {[0, 1, 2, 3, 4].map((lane) => (
          <div
            key={lane}
            style={{
              position: "absolute",
              left: 390 + lane * 92,
              bottom: -100,
              width: 12,
              height: 760,
              rotate: `${-32 + lane * 4}deg`,
              translate: `0 ${roadShift}px`,
              background: "repeating-linear-gradient(180deg,rgba(255,255,255,0.5) 0 58px,transparent 58px 180px)",
              opacity: 0.28,
            }}
          />
        ))}
        <div style={{ position: "absolute", left: 110, top: 115 }}><Rider color={COLORS.cyan} size={250} label="선두" /></div>
        <div style={{ position: "absolute", left: 315, top: 280 }}><Rider color={COLORS.yellow} size={230} label="중간" /></div>
        <div style={{ position: "absolute", left: 515, top: 445, opacity: 1 - shadow * 0.72 }}><Rider color={COLORS.red} size={210} label="후미" /></div>
        <div style={{ position: "absolute", right: 56, bottom: 46, display: "flex", alignItems: "center", gap: 14, fontSize: 24, fontWeight: 800, color: COLORS.red }}>
          <span style={{ width: 14, height: 14, borderRadius: "50%", background: COLORS.red, boxShadow: `0 0 24px ${COLORS.red}` }} />
          시야 밖
        </div>
      </div>
      <Voiceover scene="scene01" from={8} />
    </SceneShell>
  );
};
