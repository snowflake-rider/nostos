import {Easing, interpolate, useCurrentFrame} from "remotion";
import {SCENE_FRAMES} from "../Composition";
import {FadeUp, palette, SceneShell, SectionLabel} from "../design";

export const TitleScene: React.FC = () => {
  const frame = useCurrentFrame();
  return <SceneShell durationInFrames={SCENE_FRAMES.title}>
    <div style={{position: "absolute", inset: "160px 100px 120px", display: "flex", flexDirection: "column", justifyContent: "center", alignItems: "center", textAlign: "center"}}>
      <FadeUp delay={8}><SectionLabel>RIDER SAFETY MESH</SectionLabel></FadeUp>
      <FadeUp delay={18} distance={46} style={{marginTop: 38}}>
        <div style={{fontSize: 108, lineHeight: 1.12, fontWeight: 800, letterSpacing: -5}}>
          라이더의 신호가<br />
          <span style={{background: `linear-gradient(90deg, ${palette.cyan}, ${palette.blue}, ${palette.violet})`, WebkitBackgroundClip: "text", color: "transparent"}}>길 위에서 연결되도록</span>
        </div>
      </FadeUp>
      <FadeUp delay={34} style={{marginTop: 34, fontSize: 39, color: palette.muted, letterSpacing: 1}}>STM32의 판단과 ESP32-S3 Bluetooth Mesh를 잇는 NOSTOS</FadeUp>
      <div style={{marginTop: 66, display: "flex", alignItems: "center", gap: 22}}>
        {[0, 1, 2].map((index) => {
          const pulse = interpolate(frame, [48 + index * 12, 68 + index * 12], [0.7, 1], {extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1)});
          const color = index === 1 ? palette.cyan : palette.blue;
          return <div key={index} style={{width: 20, height: 20, borderRadius: 99, scale: pulse, background: color, boxShadow: `0 0 28px ${color}`}} />;
        })}
      </div>
    </div>
  </SceneShell>;
};
