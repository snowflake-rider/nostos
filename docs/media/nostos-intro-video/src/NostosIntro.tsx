import {TransitionSeries} from "@remotion/transitions";
import {SCENE_FRAMES} from "./Composition";
import {BridgeScene} from "./scenes/BridgeScene";
import {ClosingScene} from "./scenes/ClosingScene";
import {DeviceScene} from "./scenes/DeviceScene";
import {NetworkScene} from "./scenes/NetworkScene";
import {TitleScene} from "./scenes/TitleScene";
import {ToolsScene} from "./scenes/ToolsScene";

export const NostosIntro: React.FC = () => (
  <TransitionSeries>
    <TransitionSeries.Sequence durationInFrames={SCENE_FRAMES.title} name="01 — Opening"><TitleScene /></TransitionSeries.Sequence>
    <TransitionSeries.Sequence durationInFrames={SCENE_FRAMES.device} name="02 — Rider device"><DeviceScene /></TransitionSeries.Sequence>
    <TransitionSeries.Sequence durationInFrames={SCENE_FRAMES.bridge} name="03 — UART to Mesh bridge"><BridgeScene /></TransitionSeries.Sequence>
    <TransitionSeries.Sequence durationInFrames={SCENE_FRAMES.network} name="04 — Rider mesh"><NetworkScene /></TransitionSeries.Sequence>
    <TransitionSeries.Sequence durationInFrames={SCENE_FRAMES.tools} name="05 — Tools and GPS extension"><ToolsScene /></TransitionSeries.Sequence>
    <TransitionSeries.Sequence durationInFrames={SCENE_FRAMES.closing} name="06 — Closing"><ClosingScene /></TransitionSeries.Sequence>
  </TransitionSeries>
);
