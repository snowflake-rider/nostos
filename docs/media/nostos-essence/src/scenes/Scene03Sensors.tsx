import { Easing, interpolate, useCurrentFrame } from "remotion";
import { Eyebrow, Headline, SceneShell } from "../components/SceneShell";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

const SensorCard: React.FC<{
  delay: number;
  color: string;
  role: string;
  sensor: string;
  metric: string;
  icon: string;
  bars: number[];
}> = ({ delay, color, role, sensor, metric, icon, bars }) => {
  const frame = useCurrentFrame();
  const reveal = interpolate(frame, [delay, delay + 24], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
    easing: Easing.bezier(0.16, 1, 0.3, 1),
  });
  return (
    <div style={{ width: 515, height: 410, padding: "34px 36px", borderRadius: 36, border: `1px solid ${color}55`, background: "linear-gradient(160deg,rgba(17,27,50,0.96),rgba(7,13,28,0.96))", boxShadow: `0 24px 80px ${color}12`, opacity: reveal, translate: `0 ${(1 - reveal) * 34}px` }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
        <div style={{ color, fontSize: 23, fontWeight: 850, letterSpacing: 4 }}>{role}</div>
        <div style={{ width: 82, height: 82, display: "grid", placeItems: "center", borderRadius: 26, background: `${color}18`, fontSize: 44 }}>{icon}</div>
      </div>
      <div style={{ marginTop: 28, fontSize: 54, fontWeight: 850 }}>{sensor}</div>
      <div style={{ marginTop: 8, color: COLORS.muted, fontSize: 28 }}>{metric}</div>
      <div style={{ marginTop: 42, display: "flex", alignItems: "end", height: 95, gap: 12 }}>
        {bars.map((height, index) => (
          <div key={index} style={{ flex: 1, height: height * reveal, borderRadius: 9, background: `linear-gradient(180deg,${color},${color}33)` }} />
        ))}
      </div>
    </div>
  );
};

export const Scene03Sensors: React.FC = () => (
  <SceneShell label="03 / 분산 센싱" accent={COLORS.green}>
    <Eyebrow color={COLORS.green}>각 라이더가 하나의 감각을 더합니다</Eyebrow>
    <Headline size={80}>서로 다른 센서, 하나의 그룹 상태.</Headline>
    <div style={{ display: "flex", gap: 34, marginTop: 62 }}>
      <SensorCard delay={20} color={COLORS.cyan} role="A / 선두" sensor="XOSS 속도" metric="케이던스와 주행 속도" icon="↗" bars={[38, 62, 48, 82, 68, 92, 78, 98]} />
      <SensorCard delay={42} color={COLORS.yellow} role="B / 중간" sensor="DHT11" metric="온도와 습도" icon="◌" bars={[55, 58, 60, 64, 67, 66, 72, 76]} />
      <SensorCard delay={64} color={COLORS.red} role="C / 후미" sensor="MPU6050" metric="움직임과 자세" icon="◇" bars={[42, 82, 51, 92, 39, 73, 62, 88]} />
    </div>
    <div style={{ position: "absolute", right: 0, top: 38, padding: "10px 18px", borderRadius: 999, border: `1px solid ${COLORS.yellow}55`, color: COLORS.yellow, fontSize: 19, fontWeight: 800, letterSpacing: 2 }}>목표 센서 흐름</div>
    <Voiceover scene="scene03" from={8} />
  </SceneShell>
);
