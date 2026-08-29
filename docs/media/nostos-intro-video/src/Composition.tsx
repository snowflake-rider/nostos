import {Composition, Folder} from "remotion";
import {BridgeScene} from "./scenes/BridgeScene";
import {ClosingScene} from "./scenes/ClosingScene";
import {DeviceScene} from "./scenes/DeviceScene";
import {NetworkScene} from "./scenes/NetworkScene";
import {TitleScene} from "./scenes/TitleScene";
import {ToolsScene} from "./scenes/ToolsScene";
import {NostosIntro} from "./NostosIntro";

export const FPS = 30;
export const WIDTH = 1920;
export const HEIGHT = 1080;

export const SCENE_FRAMES = {
  title: 135,
  device: 195,
  bridge: 210,
  network: 210,
  tools: 180,
  closing: 120,
} as const;

export const TOTAL_FRAMES = Object.values(SCENE_FRAMES).reduce(
  (sum, frames) => sum + frames,
  0,
);

export const MyComposition = () => {
  return (
    <>
      <Folder name="NOSTOS-Scenes">
        <Composition id="TitleScene" component={TitleScene} durationInFrames={SCENE_FRAMES.title} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="DeviceScene" component={DeviceScene} durationInFrames={SCENE_FRAMES.device} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="BridgeScene" component={BridgeScene} durationInFrames={SCENE_FRAMES.bridge} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="NetworkScene" component={NetworkScene} durationInFrames={SCENE_FRAMES.network} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="ToolsScene" component={ToolsScene} durationInFrames={SCENE_FRAMES.tools} fps={FPS} width={WIDTH} height={HEIGHT} />
        <Composition id="ClosingScene" component={ClosingScene} durationInFrames={SCENE_FRAMES.closing} fps={FPS} width={WIDTH} height={HEIGHT} />
      </Folder>
      <Composition
        id="NostosIntro"
        component={NostosIntro}
        durationInFrames={TOTAL_FRAMES}
        fps={FPS}
        width={WIDTH}
        height={HEIGHT}
      />
    </>
  );
};
