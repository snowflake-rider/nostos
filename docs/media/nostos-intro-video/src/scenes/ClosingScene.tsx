import {SCENE_FRAMES} from "../Composition";
import {FadeUp, palette, SceneShell, SectionLabel} from "../design";

export const ClosingScene: React.FC = () => (
  <SceneShell durationInFrames={SCENE_FRAMES.closing} accent={palette.cyan}>
    <div style={{position: "absolute", inset: "150px 100px 110px", display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", textAlign: "center"}}>
      <FadeUp delay={4}><SectionLabel>NOSTOS · ONE SHARED CONTRACT</SectionLabel></FadeUp>
      <FadeUp delay={14} style={{marginTop: 38}}>
        <div style={{fontSize: 96, fontWeight: 800, lineHeight: 1.12, letterSpacing: -4}}>감지하고, 전달하고,<br /><span style={{color: palette.cyan}}>끝까지 검증합니다.</span></div>
      </FadeUp>
      <FadeUp delay={32} style={{display: "flex", gap: 24, marginTop: 54}}>
        {[{n: "01", t: "SENSE", s: "STM32"}, {n: "02", t: "RELAY", s: "ESP32-S3"}, {n: "03", t: "VERIFY", s: "TOOLS + TESTS"}].map((item) => <div key={item.n} style={{width: 330, padding: "22px 28px", borderRadius: 22, background: palette.panel, border: `1px solid ${palette.blue}55`, textAlign: "left"}}>
          <div style={{fontFamily: '"IBM Plex Mono", monospace', color: palette.cyan, fontSize: 20}}>{item.n}</div><div style={{fontSize: 35, fontWeight: 800, marginTop: 6}}>{item.t}</div><div style={{fontSize: 22, color: palette.muted, marginTop: 6}}>{item.s}</div>
        </div>)}
      </FadeUp>
      <FadeUp delay={54} style={{marginTop: 40, fontFamily: '"IBM Plex Mono", monospace', fontSize: 24, color: palette.muted, letterSpacing: 2}}>snowflake-rider / nostos</FadeUp>
    </div>
  </SceneShell>
);
