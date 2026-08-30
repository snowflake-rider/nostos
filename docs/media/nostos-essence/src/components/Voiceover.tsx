import { Audio } from "@remotion/media";
import { staticFile } from "remotion";

export const Voiceover: React.FC<{ scene: string; from?: number }> = ({ scene, from = 8 }) => (
  <Audio from={from} src={staticFile(`audio/${scene}.mp3`)} volume={0.94} />
);

