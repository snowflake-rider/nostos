export const FPS = 30;
export const WIDTH = 1920;
export const HEIGHT = 1080;
export const TRANSITION_FRAMES = 15;

// Scene lengths include the half-second overlap used by each transition.
export const SCENE_DURATIONS = [195, 225, 375, 405, 405, 435, 345, 240] as const;
export const TOTAL_FRAMES =
  SCENE_DURATIONS.reduce((sum, duration) => sum + duration, 0) -
  TRANSITION_FRAMES * (SCENE_DURATIONS.length - 1);

export const COLORS = {
  night: "#050914",
  panel: "#0B1224",
  panelSoft: "#111B32",
  white: "#F7FAFF",
  muted: "#94A3B8",
  cyan: "#35E6FF",
  blue: "#4F7CFF",
  green: "#35F2A0",
  yellow: "#FFD45C",
  red: "#FF476F",
  orange: "#FF8A4C",
} as const;

