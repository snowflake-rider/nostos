import { Audio, Video } from "@remotion/media";
import { Easing, interpolate, staticFile, useCurrentFrame } from "remotion";
import { Eyebrow, Headline, SceneShell } from "../components/SceneShell";
import { Voiceover } from "../components/Voiceover";
import { COLORS } from "../config";

const signals = [
  { code: "0x11", label: "속도 올리기", color: COLORS.green, sfx: "speed_up_request.mp3" },
  { code: "0x10", label: "속도 줄이기", color: COLORS.yellow, sfx: "speed_down_request.mp3" },
  { code: "0x13", label: "정지", color: COLORS.red, sfx: "stop_request.mp3" },
] as const;

export const Scene05Signals: React.FC = () => {
  const frame = useCurrentFrame();
  const active = Math.min(2, Math.floor(frame / 135));
  const local = frame % 135;
  const pulse = interpolate(local, [0, 55, 110], [0.25, 1, 0.45], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1) });
  return (
    <SceneShell label="05 / 의도가 신호가 되는 순간" accent={signals[active].color}>
      <div style={{ position: "absolute", left: 0, top: 20, width: 1040, height: 650, borderRadius: 42, overflow: "hidden", border: "1px solid rgba(255,255,255,0.13)", boxShadow: "0 30px 100px rgba(0,0,0,0.45)" }}>
        <Video src={staticFile("video/nostos-ride-signals.mp4")} playbackRate={2} muted objectFit="cover" style={{ width: "100%", height: "100%", filter: "blur(2px) saturate(0.7)", opacity: 0.55 }} />
        <div style={{ position: "absolute", inset: 0, background: "linear-gradient(90deg,rgba(5,9,20,0.22),rgba(5,9,20,0.86))" }} />
        <div style={{ position: "absolute", left: 34, top: 30, padding: "10px 16px", borderRadius: 999, background: "rgba(5,9,20,0.82)", fontSize: 18, letterSpacing: 2 }}>프로젝트 신호 시퀀스</div>
      </div>
      <div style={{ position: "absolute", right: 0, top: 28, width: 560 }}>
        <Eyebrow color={signals[active].color}>한 번의 입력, 그룹 전체의 의미</Eyebrow>
        <Headline size={68} width={560}>모두가 이해하는<br />신호.</Headline>
        <div style={{ marginTop: 48, display: "flex", flexDirection: "column", gap: 16 }}>
          {signals.map((signal, index) => {
            const selected = index === active;
            return (
              <div key={signal.code} style={{ display: "flex", alignItems: "center", gap: 22, padding: "20px 24px", borderRadius: 24, border: `1px solid ${selected ? signal.color : "rgba(255,255,255,0.12)"}`, background: selected ? `${signal.color}18` : "rgba(11,18,36,0.55)", opacity: selected ? pulse : 0.46, scale: selected ? 1.03 : 1 }}>
                <div style={{ color: signal.color, fontSize: 25, fontFamily: "SFMono-Regular, Menlo, monospace", fontWeight: 800 }}>{signal.code}</div>
                <div style={{ fontSize: 31, fontWeight: 850 }}>{signal.label}</div>
              </div>
            );
          })}
        </div>
      </div>
      <Audio from={18} src={staticFile(`sfx/${signals[0].sfx}`)} volume={0.22} />
      <Audio from={153} src={staticFile(`sfx/${signals[1].sfx}`)} volume={0.22} />
      <Audio from={288} src={staticFile(`sfx/${signals[2].sfx}`)} volume={0.25} />
      <Voiceover scene="scene05" from={8} />
    </SceneShell>
  );
};
