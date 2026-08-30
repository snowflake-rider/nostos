import "./index.css";
import { Composition, Folder } from "remotion";
import { FPS, HEIGHT, SCENE_DURATIONS, TOTAL_FRAMES, WIDTH } from "./config";
import { NostosEssence } from "./NostosEssence";
import { Scene01Hook } from "./scenes/Scene01Hook";
import { Scene02Connect } from "./scenes/Scene02Connect";
import { Scene03Sensors } from "./scenes/Scene03Sensors";
import { Scene04Architecture } from "./scenes/Scene04Architecture";
import { Scene05Signals } from "./scenes/Scene05Signals";
import { Scene06Safety } from "./scenes/Scene06Safety";
import { Scene07SharedState } from "./scenes/Scene07SharedState";
import { Scene08Finale } from "./scenes/Scene08Finale";

export const RemotionRoot: React.FC = () => {
  return (
    <>
      <Folder name="Nostos-Essence-Scenes">
        <Composition id="Scene01-Hook" component={Scene01Hook} durationInFrames={SCENE_DURATIONS[0]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene02-Connect" component={Scene02Connect} durationInFrames={SCENE_DURATIONS[1]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene03-Sensors" component={Scene03Sensors} durationInFrames={SCENE_DURATIONS[2]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene04-Architecture" component={Scene04Architecture} durationInFrames={SCENE_DURATIONS[3]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene05-Signals" component={Scene05Signals} durationInFrames={SCENE_DURATIONS[4]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene06-Safety" component={Scene06Safety} durationInFrames={SCENE_DURATIONS[5]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene07-Shared-State" component={Scene07SharedState} durationInFrames={SCENE_DURATIONS[6]} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="Scene08-Finale" component={Scene08Finale} durationInFrames={SCENE_DURATIONS[7]} fps={FPS} width={WIDTH} height={HEIGHT} />
      </Folder>
      <Composition
        id="NostosEssence"
        component={NostosEssence}
        durationInFrames={TOTAL_FRAMES}
        fps={FPS}
        width={WIDTH}
        height={HEIGHT}
      />
    </>
  );
};
