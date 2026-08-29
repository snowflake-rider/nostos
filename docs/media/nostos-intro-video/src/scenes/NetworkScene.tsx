import {interpolate, useCurrentFrame} from "remotion";
import {SCENE_FRAMES} from "../Composition";
import {FadeUp, palette, SceneShell, SectionLabel} from "../design";

const positions = [
  {x: 250, y: 330, name: "RIDER A", color: palette.cyan},
  {x: 780, y: 560, name: "RIDER B", color: palette.blue},
  {x: 1320, y: 310, name: "RIDER C", color: palette.violet},
];

export const NetworkScene: React.FC = () => {
  const frame = useCurrentFrame();
  return <SceneShell durationInFrames={SCENE_FRAMES.network} accent={palette.violet}>
    <div style={{position: "absolute", inset: "135px 80px 95px"}}>
      <FadeUp delay={6}><SectionLabel color={palette.violet}>03 · RIDER MESH</SectionLabel></FadeUp>
      <FadeUp delay={16} style={{marginTop: 28}}>
        <div style={{fontSize: 82, fontWeight: 800, letterSpacing: -3}}>한 사람의 이벤트를, 여러 노드가 공유</div>
        <div style={{fontSize: 36, color: palette.muted, marginTop: 14}}>ESP32-S3 노드들이 Bluetooth Mesh 그룹에서 이벤트를 송수신합니다.</div>
      </FadeUp>
      <div style={{position: "absolute", left: 60, right: 60, top: 270, bottom: 20}}>
        <svg width="100%" height="100%" viewBox="0 0 1600 630" style={{position: "absolute", inset: 0}}>
          <defs><linearGradient id="meshLine" x1="0" x2="1"><stop stopColor={palette.cyan} /><stop offset="0.5" stopColor={palette.blue} /><stop offset="1" stopColor={palette.violet} /></linearGradient></defs>
          <path d="M350 380 L870 570 L1400 360 L350 380" fill="none" stroke="url(#meshLine)" strokeWidth="4" strokeDasharray="18 14" opacity="0.7" strokeDashoffset={-frame * 2.2} />
        </svg>
        {positions.map((node, index) => {
          const appear = interpolate(frame, [42 + index * 15, 67 + index * 15], [0, 1], {extrapolateLeft: "clamp", extrapolateRight: "clamp"});
          const ring = 1 + ((frame + index * 17) % 55) / 55;
          return <div key={node.name} style={{position: "absolute", left: node.x, top: node.y, translate: "-50% -50%", opacity: appear}}>
            <div style={{position: "absolute", left: "50%", top: "50%", translate: "-50% -50%", width: 150 * ring, height: 150 * ring, borderRadius: 999, border: `2px solid ${node.color}`, opacity: 0.45 * (2 - ring)}} />
            <div style={{width: 190, height: 190, borderRadius: 999, display: "flex", alignItems: "center", justifyContent: "center", flexDirection: "column", background: `radial-gradient(circle, ${node.color}22, #0b1326 72%)`, border: `2px solid ${node.color}`, boxShadow: `0 0 70px ${node.color}33`}}>
              <div style={{fontFamily: '"IBM Plex Mono", monospace', fontSize: 20, color: node.color, letterSpacing: 2}}>{node.name}</div>
              <div style={{fontSize: 30, fontWeight: 750, marginTop: 10}}>ESP32-S3</div>
            </div>
          </div>;
        })}
        <FadeUp delay={114} style={{position: "absolute", left: 520, top: 292, width: 560, padding: "22px 28px", borderRadius: 22, textAlign: "center", background: "rgba(5,9,20,0.92)", border: `1px solid ${palette.blue}88`}}>
          <div style={{fontSize: 28, color: palette.cyan, fontFamily: '"IBM Plex Mono", monospace'}}>EVENT 0x13</div>
          <div style={{fontSize: 38, fontWeight: 750, marginTop: 8}}>정지 요청 전달</div>
        </FadeUp>
      </div>
    </div>
  </SceneShell>;
};
