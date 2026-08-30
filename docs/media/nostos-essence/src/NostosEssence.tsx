import { Audio } from "@remotion/media";
import { TransitionSeries, linearTiming } from "@remotion/transitions";
import { fade } from "@remotion/transitions/fade";
import { AbsoluteFill, staticFile } from "remotion";
import { CaptionOverlay } from "./components/CaptionOverlay";
import { SCENE_DURATIONS, TRANSITION_FRAMES } from "./config";
import { Scene01Hook } from "./scenes/Scene01Hook";
import { Scene02Connect } from "./scenes/Scene02Connect";
import { Scene03Sensors } from "./scenes/Scene03Sensors";
import { Scene04Architecture } from "./scenes/Scene04Architecture";
import { Scene05Signals } from "./scenes/Scene05Signals";
import { Scene06Safety } from "./scenes/Scene06Safety";
import { Scene07SharedState } from "./scenes/Scene07SharedState";
import { Scene08Finale } from "./scenes/Scene08Finale";

const transition = linearTiming({ durationInFrames: TRANSITION_FRAMES });

export const NostosEssence: React.FC = () => (
  <AbsoluteFill>
    <TransitionSeries>
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[0]} name="Hook"><Scene01Hook /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[1]} name="Connect"><Scene02Connect /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[2]} name="Sensors"><Scene03Sensors /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[3]} name="Architecture"><Scene04Architecture /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[4]} name="Signals"><Scene05Signals /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[5]} name="Safety"><Scene06Safety /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[6]} name="Shared state"><Scene07SharedState /></TransitionSeries.Sequence>
      <TransitionSeries.Transition presentation={fade()} timing={transition} />
      <TransitionSeries.Sequence durationInFrames={SCENE_DURATIONS[7]} name="Finale"><Scene08Finale /></TransitionSeries.Sequence>
    </TransitionSeries>
    <Audio src={staticFile("audio/ambient.mp3")} volume={0.19} />
    <CaptionOverlay />
  </AbsoluteFill>
);

