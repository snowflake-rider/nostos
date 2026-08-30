import { interpolate, useCurrentFrame } from "remotion";
import { COLORS } from "../config";

export const Rider: React.FC<{
  color?: string;
  label?: string;
  size?: number;
  tilt?: number;
  direction?: "left" | "right";
}> = ({ color = COLORS.cyan, label, size = 260, tilt = 0, direction = "right" }) => {
  const frame = useCurrentFrame();
  const pedal = (frame * 8) % 360;
  const bob = Math.sin(frame / 6) * 3;

  return (
    <div style={{ width: size, position: "relative", translate: `0 ${bob}px` }}>
      <svg
        width={size}
        height={(size * 0.68)}
        viewBox="0 0 300 205"
        style={{ scale: direction === "left" ? "-1 1" : "1 1", overflow: "visible" }}
      >
        <g transform={`rotate(${tilt} 150 105)`}>
          <circle cx="65" cy="145" r="42" fill="none" stroke="#DCE7F7" strokeWidth="7" opacity="0.95" />
          <circle cx="235" cy="145" r="42" fill="none" stroke="#DCE7F7" strokeWidth="7" opacity="0.95" />
          <path d="M65 145 L122 80 L162 145 L95 145 L138 105 L208 106 L235 145" fill="none" stroke={color} strokeWidth="10" strokeLinecap="round" strokeLinejoin="round" />
          <path d="M122 80 L107 58 M102 58 H138 M208 106 L196 78 M185 78 H216" fill="none" stroke="#DCE7F7" strokeWidth="8" strokeLinecap="round" />
          <circle cx="147" cy="43" r="20" fill={color} />
          <path d="M143 64 L126 95 L175 104 L197 80 M130 88 L96 82" fill="none" stroke={color} strokeWidth="13" strokeLinecap="round" strokeLinejoin="round" />
          <g transform={`rotate(${pedal} 162 145)`}>
            <path d="M162 145 L190 145 M162 145 L134 145" stroke="#DCE7F7" strokeWidth="7" strokeLinecap="round" />
          </g>
          <circle cx="162" cy="145" r="9" fill="#DCE7F7" />
        </g>
      </svg>
      {label ? (
        <div style={{ textAlign: "center", marginTop: -10, fontSize: 22, fontWeight: 800, letterSpacing: 3, color }}>
          {label}
        </div>
      ) : null}
    </div>
  );
};

export const SignalPulse: React.FC<{
  color: string;
  progress: number;
  size?: number;
}> = ({ color, progress, size = 300 }) => {
  const radius = interpolate(progress, [0, 1], [18, size / 2]);
  const opacity = interpolate(progress, [0, 0.75, 1], [0.9, 0.3, 0]);
  return (
    <div
      style={{
        position: "absolute",
        width: radius * 2,
        height: radius * 2,
        borderRadius: "50%",
        border: `5px solid ${color}`,
        boxShadow: `0 0 42px ${color}66`,
        opacity,
        left: "50%",
        top: "50%",
        translate: "-50% -50%",
      }}
    />
  );
};

