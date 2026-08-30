import type { ReactNode } from "react";
import { AbsoluteFill, Easing, interpolate, useCurrentFrame } from "remotion";
import { COLORS } from "../config";

export const SceneShell: React.FC<{
  children: ReactNode;
  accent?: string;
  label?: string;
}> = ({ children, accent = COLORS.cyan, label }) => {
  const frame = useCurrentFrame();
  const reveal = interpolate(frame, [0, 18], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
    easing: Easing.bezier(0.16, 1, 0.3, 1),
  });
  const gridOffset = (frame * 0.45) % 64;

  return (
    <AbsoluteFill
      style={{
        overflow: "hidden",
        color: COLORS.white,
        background: `radial-gradient(circle at 72% 18%, ${accent}18 0%, transparent 32%), linear-gradient(145deg, #07101F 0%, ${COLORS.night} 68%)`,
      }}
    >
      <AbsoluteFill style={{ opacity: 0.13 }}>
        <svg width="100%" height="100%" aria-hidden="true">
          <defs>
            <pattern id="grid" width="64" height="64" patternUnits="userSpaceOnUse" x={gridOffset} y={gridOffset}>
              <path d="M64 0H0V64" fill="none" stroke={accent} strokeWidth="1" />
            </pattern>
          </defs>
          <rect width="100%" height="100%" fill="url(#grid)" />
        </svg>
      </AbsoluteFill>
      <AbsoluteFill
        style={{
          background: "linear-gradient(180deg, rgba(5,9,20,0.08), rgba(5,9,20,0.72))",
        }}
      />
      {label ? (
        <div
          style={{
            position: "absolute",
            top: 72,
            left: 96,
            fontSize: 24,
            fontWeight: 750,
            letterSpacing: 6,
            color: accent,
            opacity: reveal,
          }}
        >
          {label}
        </div>
      ) : null}
      <div
        style={{
          position: "absolute",
          inset: "112px 96px 176px",
          opacity: reveal,
          translate: `0 ${interpolate(frame, [0, 18], [18, 0], {
            extrapolateLeft: "clamp",
            extrapolateRight: "clamp",
            easing: Easing.bezier(0.16, 1, 0.3, 1),
          })}px`,
        }}
      >
        {children}
      </div>
      <div
        style={{
          position: "absolute",
          right: 76,
          top: 66,
          fontSize: 18,
          letterSpacing: 4,
          color: COLORS.muted,
        }}
      >
        NOSTOS / 콘셉트 영상
      </div>
    </AbsoluteFill>
  );
};

export const Eyebrow: React.FC<{ children: ReactNode; color?: string }> = ({
  children,
  color = COLORS.cyan,
}) => (
  <div style={{ color, fontSize: 24, fontWeight: 800, letterSpacing: 5, textTransform: "uppercase" }}>
    {children}
  </div>
);

export const Headline: React.FC<{
  children: ReactNode;
  size?: number;
  width?: number | string;
}> = ({ children, size = 92, width = 1080 }) => (
  <div
    style={{
      marginTop: 20,
      maxWidth: width,
      fontSize: size,
      fontWeight: 850,
      letterSpacing: -4,
      lineHeight: 0.96,
    }}
  >
    {children}
  </div>
);
