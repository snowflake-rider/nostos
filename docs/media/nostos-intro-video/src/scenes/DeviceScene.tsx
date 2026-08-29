import {SCENE_FRAMES} from "../Composition";
import {FadeUp, FlowArrow, NodeCard, palette, SceneShell, SectionLabel} from "../design";

export const DeviceScene: React.FC = () => (
  <SceneShell durationInFrames={SCENE_FRAMES.device} accent={palette.lime}>
    <div style={{position: "absolute", inset: "135px 80px 100px"}}>
      <FadeUp delay={6}><SectionLabel color={palette.lime}>01 · LOCAL INTELLIGENCE</SectionLabel></FadeUp>
      <FadeUp delay={16} style={{marginTop: 28}}>
        <div style={{fontSize: 82, fontWeight: 800, letterSpacing: -3}}>판단은 라이더 곁에서</div>
        <div style={{fontSize: 36, color: palette.muted, marginTop: 14}}>버튼·센서를 읽고, 어떤 이벤트인지 결정하고, 로컬 출력을 제어합니다.</div>
      </FadeUp>
      <div style={{marginTop: 78, display: "flex", alignItems: "center", justifyContent: "center"}}>
        <FadeUp delay={38}><NodeCard eyebrow="INPUT" title="버튼 · 센서" body="감속, 정지, 안전 주의, 낙차·SOS 등의 입력" color={palette.amber} /></FadeUp>
        <FlowArrow color={palette.lime} width={150} delay={54} />
        <FadeUp delay={52}><NodeCard eyebrow="DECISION" title="STM32 F411" body="센서 판정과 메시지 선택을 담당하는 중심 제어기" color={palette.lime} active width={430} /></FadeUp>
        <FlowArrow color={palette.cyan} width={150} delay={70} />
        <FadeUp delay={66}><NodeCard eyebrow="LOCAL OUTPUT" title="LED · 부저 · 음성" body="수신 알림과 현장 피드백을 장치에서 직접 출력" color={palette.cyan} /></FadeUp>
      </div>
      <FadeUp delay={104} style={{marginTop: 42, textAlign: "center", fontSize: 29, color: palette.muted}}>
        핵심 원칙: <span style={{color: palette.text, fontWeight: 700}}>STM32가 내용을 결정하고, 무선 모듈은 전달에 집중합니다.</span>
      </FadeUp>
    </div>
  </SceneShell>
);
