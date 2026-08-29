import {SCENE_FRAMES} from "../Composition";
import {FadeUp, NodeCard, palette, SceneShell, SectionLabel} from "../design";

export const ToolsScene: React.FC = () => (
  <SceneShell durationInFrames={SCENE_FRAMES.tools} accent={palette.amber}>
    <div style={{position: "absolute", inset: "135px 80px 95px"}}>
      <FadeUp delay={6}><SectionLabel color={palette.amber}>04 · OBSERVE & EXTEND</SectionLabel></FadeUp>
      <FadeUp delay={16} style={{marginTop: 28}}>
        <div style={{fontSize: 82, fontWeight: 800, letterSpacing: -3}}>보이는 로그, 확장되는 위치 정보</div>
        <div style={{fontSize: 36, color: palette.muted, marginTop: 14}}>로컬 도구와 iPhone 앱이 개발·검증 흐름을 넓힙니다.</div>
      </FadeUp>
      <div style={{display: "flex", justifyContent: "center", gap: 72, marginTop: 78}}>
        <FadeUp delay={40}>
          <NodeCard eyebrow="MAC · LOCAL USB" title="Mesh Console" body="세 ESP32의 상태와 UART 로그를 한 화면에서 관찰" color={palette.amber} active width={670} />
          <div style={{marginTop: 20, borderRadius: 20, padding: "18px 24px", background: "#070b14", border: "1px solid rgba(255,255,255,0.08)", fontFamily: '"IBM Plex Mono", monospace', fontSize: 22, lineHeight: 1.65, color: palette.muted}}>
            <div><span style={{color: palette.cyan}}>D6</span> UART_RX id=0x13</div>
            <div><span style={{color: palette.lime}}>B6</span> MESH_RX stop_request</div>
            <div><span style={{color: palette.violet}}>76</span> STATUS event_ready=1</div>
          </div>
        </FadeUp>
        <FadeUp delay={58}>
          <NodeCard eyebrow="IPHONE · BLUETOOTH" title="GPS Mesh" body="위치와 지도를 표시하고, 사용자가 시작하면 Mesh 그룹으로 공유" color={palette.violet} width={670} />
          <div style={{marginTop: 20, borderRadius: 20, padding: "18px 24px", background: "#070b14", border: "1px solid rgba(255,255,255,0.08)", display: "flex", justifyContent: "space-between", fontSize: 25}}>
            <span style={{color: palette.muted}}>37.5665, 126.9780</span><span style={{color: palette.violet, fontWeight: 750}}>GPS → MESH</span>
          </div>
        </FadeUp>
      </div>
      <FadeUp delay={112} style={{marginTop: 42, padding: "18px 28px", borderRadius: 20, background: `${palette.amber}10`, border: `1px solid ${palette.amber}55`, fontSize: 27, textAlign: "center", color: palette.muted}}>
        화면·호스트 테스트와 실물 수신 증거는 구분합니다. <span style={{color: palette.text, fontWeight: 700}}>전송 요청 성공 ≠ 상대 수신 보장</span>
      </FadeUp>
    </div>
  </SceneShell>
);
