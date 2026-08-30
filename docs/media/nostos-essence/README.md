# NOSTOS Essence Film

An 84-second cinematic product film that distills NOSTOS into one idea:
three riders share speed, environmental, motion, and safety information as one
connected group state.

The film is built with [Remotion](https://www.remotion.dev/) at 1920×1080,
30 fps. English voice-over, Korean on-screen copy and captions, ambient sound, device sound effects, and
the existing ride-signal footage are assembled into the `NostosEssence`
composition.

## Story structure

1. **Hook** (`Scene01-Hook`) — A hazard begins when one rider cannot see what
   is happening elsewhere in the group.
2. **Connect** (`Scene02-Connect`) — Three bicycles become one connected sense.
3. **Sensors** (`Scene03-Sensors`) — The lead, middle, and tail riders contribute
   speed, environment, and motion data.
4. **Architecture** (`Scene04-Architecture`) — Sensor and button input flows
   through STM32, UART, ESP32-S3, and Bluetooth Mesh to local outputs.
5. **Signals** (`Scene05-Signals`) — Speed up, slow down, and stop requests move
   through the riding line.
6. **Safety** (`Scene06-Safety`) — A fall or SOS becomes an urgent shared state.
7. **Shared state** (`Scene07-Shared-State`) — Every rider sees the group rather
   than only a local sensor.
8. **Finale** (`Scene08-Finale`) — “Three riders. One shared state. Leave
   together. Return together.”

Each scene is also registered as a standalone composition for focused preview
and review.

## Validation boundary

This is a **concept film**, not evidence that every depicted scenario has been
validated on physical hardware. The visual architecture and interactions
represent the intended NOSTOS experience. End-to-end validation of the complete
three-node sensor-sharing, relay, and shared-state flow remains a target for
hardware testing.

The film therefore includes the disclosure:

> CONCEPT FILM · 3-NODE SENSOR E2E VALIDATION IN PROGRESS

## Commands

Run all commands from this directory.

```console
npm install
npm run lint
npm run check-media
npm run dev
npm run stills
npm run render
```

- `npm install` installs the locked Remotion and React dependencies.
- `npm run lint` runs ESLint and the TypeScript compiler check.
- `npm run check-media` verifies that every required caption, audio, sound
  effect, and source-video file exists and is non-empty.
- `npm run dev` opens Remotion Studio for interactive preview.
- `npm run stills` renders a representative frame from each of the eight scene
  compositions.
- `npm run render` renders the full `NostosEssence` composition as H.264 video.

The existing `npm run build` and `npm run upgrade` commands remain available for
bundling the project and upgrading Remotion.

## Outputs

- Full film: `out/nostos-essence.mp4`
- Cover image: `out/nostos-essence-cover.png`
- Scene review frames: `out/stills/scene01-hook.png` through
  `out/stills/scene08-finale.png`

Generated output is written under `out/`. Source media remains under `public/`.
