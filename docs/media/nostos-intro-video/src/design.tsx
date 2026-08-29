import type {CSSProperties, ReactNode} from "react";
import {AbsoluteFill, Easing, interpolate, useCurrentFrame} from "remotion";

export const palette = {
  bg: "#050914",
  panel: "rgba(15, 25, 47, 0.78)",
  panelStrong: "#101b33",
  text: "#f6f8ff",
  muted: "#9ba9c4",
  cyan: "#55e6ff",
  blue: "#5b8cff",
  violet: "#9b7bff",
  lime: "#a7f35a",
  amber: "#ffc857",
  red: "#ff6b7a",
};

export const SceneShell: React.FC<{children: ReactNode; durationInFrames: number; accent?: string}> = ({children, durationInFrames, accent = palette.cyan}) => {
  const frame = useCurrentFrame();
  return (
    <AbsoluteFill style={{
      opacity: interpolate(frame, [0, 12, durationInFrames - 14, durationInFrames - 1], [0, 1, 1, 0], {extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1)}),
      color: palette.text,
      fontFamily: '"Apple SD Gothic Neo", "Malgun Gothic", Arial, sans-serif',
      overflow: "hidden",
      background: "radial-gradient(circle at 78% 18%, rgba(91,140,255,0.18), transparent 34%), radial-gradient(circle at 15% 84%, rgba(85,230,255,0.10), transparent 32%), #050914",
    }}>
      <AbsoluteFill style={{opacity: 0.18, backgroundImage: "linear-gradient(rgba(155,169,196,0.16) 1px, transparent 1px), linear-gradient(90deg, rgba(155,169,196,0.16) 1px, transparent 1px)", backgroundSize: "64px 64px"}} />
      <div style={{position: "absolute", left: 80, top: 54, display: "flex", alignItems: "center", gap: 16, fontFamily: '"IBM Plex Mono", monospace', fontSize: 23, fontWeight: 600, letterSpacing: 4, color: palette.muted}}>
        <span style={{width: 12, height: 12, borderRadius: 99, background: accent, boxShadow: `0 0 22px ${accent}`}} />NOSTOS
      </div>
      <div style={{position: "relative", width: "100%", height: "100%"}}>{children}</div>
    </AbsoluteFill>
  );
};

export const FadeUp: React.FC<{children: ReactNode; delay?: number; distance?: number; style?: CSSProperties}> = ({children, delay = 0, distance = 32, style}) => {
  const frame = useCurrentFrame();
  return <div style={{...style,
    opacity: interpolate(frame, [delay, delay + 20], [0, 1], {extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1)}),
    translate: `0 ${interpolate(frame, [delay, delay + 22], [distance, 0], {extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1)})}px`,
  }}>{children}</div>;
};

export const SectionLabel: React.FC<{children: ReactNode; color?: string}> = ({children, color = palette.cyan}) => (
  <div style={{display: "inline-flex", alignItems: "center", gap: 12, padding: "10px 18px", border: `1px solid ${color}66`, borderRadius: 999, color, background: `${color}12`, fontFamily: '"IBM Plex Mono", monospace', fontSize: 21, fontWeight: 600, letterSpacing: 2}}>{children}</div>
);

export const NodeCard: React.FC<{eyebrow: string; title: string; body: string; color: string; active?: boolean; width?: number}> = ({eyebrow, title, body, color, active = false, width = 390}) => (
  <div style={{width, minHeight: 250, borderRadius: 28, padding: "30px 32px", border: `1px solid ${color}${active ? "cc" : "55"}`, background: active ? `linear-gradient(145deg, ${color}22, rgba(15,25,47,0.94))` : palette.panel, boxShadow: active ? `0 24px 90px ${color}22` : "0 20px 70px rgba(0,0,0,0.24)"}}>
    <div style={{fontFamily: '"IBM Plex Mono", monospace', fontSize: 19, letterSpacing: 2, color}}>{eyebrow}</div>
    <div style={{fontSize: 45, fontWeight: 750, marginTop: 14, lineHeight: 1.18}}>{title}</div>
    <div style={{fontSize: 27, color: palette.muted, marginTop: 16, lineHeight: 1.55}}>{body}</div>
  </div>
);

export const FlowArrow: React.FC<{color?: string; width?: number; delay?: number; label?: string}> = ({color = palette.cyan, width = 180, delay = 0, label}) => {
  const frame = useCurrentFrame();
  const progress = interpolate(frame, [delay, delay + 42], [0, 1], {extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.bezier(0.16, 1, 0.3, 1)});
  const dot = ((Math.max(0, frame - delay) % 48) / 48) * Math.max(0, width - 12);
  return <div style={{width, position: "relative", paddingTop: label ? 46 : 0}}>
    {label ? <div style={{position: "absolute", top: 0, width: "100%", textAlign: "center", fontSize: 21, color: palette.muted}}>{label}</div> : null}
    <div style={{height: 2, width: `${progress * 100}%`, background: `linear-gradient(90deg, ${color}22, ${color})`, position: "relative"}}>
      <div style={{position: "absolute", right: -2, top: -6, width: 13, height: 13, borderTop: `2px solid ${color}`, borderRight: `2px solid ${color}`, rotate: "45deg"}} />
      {progress > 0.98 ? <div style={{position: "absolute", left: dot, top: -5, width: 11, height: 11, borderRadius: 99, background: color, boxShadow: `0 0 18px ${color}`}} /> : null}
    </div>
  </div>;
};
