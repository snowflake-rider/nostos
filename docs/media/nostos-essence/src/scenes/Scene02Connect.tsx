import { Easing, interpolate, useCurrentFrame } from "remotion";
import { Rider, SignalPulse } from "../components/Rider";
import { SceneShell } from "../components/SceneShell";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

export const Scene02Connect: React.FC = () => {
  const frame = useCurrentFrame();
  const connection = interpolate(frame, [25, 105], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
    easing: Easing.bezier(0.16, 1, 0.3, 1),
  });
  const pulse = (frame % 75) / 75;

  return (
    <SceneShell label="02 / 하나로 연결된 감각">
      <div style={{ textAlign: "center", fontSize: 156, fontWeight: 900, letterSpacing: 18, lineHeight: 1, marginTop: -10 }}>
        NOSTOS
      </div>
      <div style={{ textAlign: "center", marginTop: 20, fontSize: 36, color: COLORS.muted, letterSpacing: 3 }}>
        세 명의 라이더 · 하나의 공유 상태
      </div>
      <div style={{ position: "absolute", left: 150, right: 150, top: 330, height: 260 }}>
        <div style={{ position: "absolute", left: 190, right: 190, top: 109, height: 5, background: "rgba(53,230,255,0.16)", overflow: "hidden" }}>
          <div style={{ width: `${connection * 100}%`, height: "100%", background: `linear-gradient(90deg,${COLORS.cyan},${COLORS.green})`, boxShadow: `0 0 25px ${COLORS.cyan}` }} />
        </div>
        {[
          { left: 0, color: COLORS.cyan, label: "선두" },
          { left: 525, color: COLORS.yellow, label: "중간" },
          { left: 1050, color: COLORS.red, label: "후미" },
        ].map((rider, index) => (
          <div key={rider.label} style={{ position: "absolute", left: rider.left, top: 0, opacity: interpolate(frame, [10 + index * 18, 34 + index * 18], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" }) }}>
            <div style={{ position: "relative" }}>
              <Rider color={rider.color} size={300} label={rider.label} />
              <SignalPulse color={rider.color} progress={pulse} size={230} />
            </div>
          </div>
        ))}
      </div>
      <Voiceover scene="scene02" from={14} />
    </SceneShell>
  );
};
