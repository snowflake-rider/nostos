import {interpolate, useCurrentFrame} from "remotion";
import {SCENE_FRAMES} from "../Composition";
import {FadeUp, FlowArrow, NodeCard, palette, SceneShell, SectionLabel} from "../design";

export const BridgeScene: React.FC = () => {
  const frame = useCurrentFrame();
  const id = frame > 95 ? "01 13" : "13";
  return <SceneShell durationInFrames={SCENE_FRAMES.bridge} accent={palette.blue}>
    <div style={{position: "absolute", inset: "135px 80px 100px"}}>
      <FadeUp delay={6}><SectionLabel color={palette.blue}>02 · EVENT BRIDGE</SectionLabel></FadeUp>
      <FadeUp delay={16} style={{marginTop: 28}}>
        <div style={{fontSize: 82, fontWeight: 800, letterSpacing: -3}}>한 바이트의 판단이 Mesh 이벤트로</div>
        <div style={{fontSize: 36, color: palette.muted, marginTop: 14}}>공통 프로토콜이 STM32와 ESP32-S3 사이의 메시지 계약을 지킵니다.</div>
      </FadeUp>
      <div style={{marginTop: 72, display: "flex", alignItems: "center", justifyContent: "center"}}>
        <FadeUp delay={38}><NodeCard eyebrow="SOURCE" title="STM32" body="정지 요청을 선택" color={palette.lime} width={330} /></FadeUp>
        <FlowArrow color={palette.amber} width={210} delay={54} label="UART · 1 byte" />
        <FadeUp delay={58} style={{position: "relative"}}>
          <NodeCard eyebrow="BRIDGE" title="ESP32-S3" body="UART ↔ Mesh 전달" color={palette.blue} active width={400} />
          <div style={{position: "absolute", left: 34, right: 34, bottom: -45, height: 72, borderRadius: 18, background: "#070c18", border: `1px solid ${palette.blue}88`, display: "flex", alignItems: "center", justifyContent: "center", fontFamily: '"IBM Plex Mono", monospace', fontSize: 30, color: palette.cyan}}>payload&nbsp; {id}</div>
        </FadeUp>
        <FlowArrow color={palette.cyan} width={210} delay={88} label="Mesh · version + ID" />
        <FadeUp delay={94}><NodeCard eyebrow="AIR" title="Bluetooth Mesh" body="다른 라이더의 노드로 전달" color={palette.cyan} width={370} /></FadeUp>
      </div>
      <FadeUp delay={132} style={{marginTop: 74, display: "flex", justifyContent: "center", gap: 24}}>
        {["0x10 감속", "0x13 정지", "0x30 낙차", "0x31 SOS"].map((label, index) => <div key={label} style={{padding: "14px 24px", borderRadius: 16, background: index === 1 ? `${palette.red}22` : palette.panel, border: `1px solid ${index === 1 ? palette.red : palette.muted}55`, fontSize: 25, color: index === 1 ? palette.red : palette.muted}}>{label}</div>)}
      </FadeUp>
      <div style={{position: "absolute", right: 0, bottom: 0, opacity: interpolate(frame, [150, 175], [0, 1], {extrapolateLeft: "clamp", extrapolateRight: "clamp"}), color: palette.muted, fontSize: 22}}>현재 기본 경로는 v1 · v2는 명시적으로 선택</div>
    </div>
  </SceneShell>;
};
