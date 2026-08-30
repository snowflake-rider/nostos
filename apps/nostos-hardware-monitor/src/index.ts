import { createCliRenderer, type KeyEvent } from "@opentui/core";

import { discoverTools, loadBoards, loadMonitorLayout } from "./config.js";
import { createDemoSnapshot } from "./demo.js";
import type { Board } from "./model.js";
import { HardwareMonitor } from "./monitor.js";
import { Dashboard } from "./ui.js";

interface Options {
  node?: string;
  intervalMs: number;
  demo: boolean;
  list: boolean;
  help: boolean;
}

const HELP = `NOSTOS Hardware Monitor

Usage:
  bun start -- --node NODE_ID [--interval 250]
  bun start -- --list
  bun start -- --demo

Options:
  --node ID       STM32 node from firmware/inventory/boards.local.json
  --interval MS   SWD sampling interval, 100..2000 ms (default: 250)
  --demo          render changing values without hardware
  --list          print registered STM32 nodes
  --help          show this help
`;

function parseOptions(args: string[]): Options {
  const options: Options = {
    intervalMs: 250,
    demo: false,
    list: false,
    help: false,
  };
  for (let index = 0; index < args.length; index += 1) {
    const argument = args[index]!;
    if (argument === "--node") {
      const value = args[++index];
      if (!value) throw new Error("--node requires an ID");
      options.node = value;
    } else if (argument === "--interval") {
      const value = Number.parseInt(args[++index] ?? "", 10);
      if (!Number.isInteger(value) || value < 100 || value > 2000) {
        throw new Error("--interval must be between 100 and 2000 ms");
      }
      options.intervalMs = value;
    } else if (argument === "--demo") {
      options.demo = true;
    } else if (argument === "--list") {
      options.list = true;
    } else if (argument === "--help" || argument === "-h") {
      options.help = true;
    } else {
      throw new Error(`unknown option: ${argument}`);
    }
  }
  return options;
}

async function main(): Promise<void> {
  const options = parseOptions(Bun.argv.slice(2));
  if (options.help) {
    console.log(HELP);
    return;
  }

  const boards = await loadBoards();
  if (options.list) {
    for (const board of boards) console.log(`${board.id}\t${board.serial}`);
    return;
  }

  const board: Board = options.demo
    ? { id: "demo-stm32", serial: "DEMO", deviceType: "STM32F411xC_xE" }
    : (boards.find((candidate) => candidate.id === options.node) ?? boards[0]!);
  if (!options.demo && options.node && board.id !== options.node) {
    throw new Error(`STM32 node is not registered: ${options.node}`);
  }

  const renderer = await createCliRenderer({
    exitOnCtrlC: false,
    targetFps: 15,
    maxFps: 30,
    useMouse: false,
  });
  const dashboard = new Dashboard(renderer, board, options.intervalMs);

  let monitor: HardwareMonitor | undefined;
  let demoTimer: ReturnType<typeof setInterval> | undefined;
  let closing = false;

  const cleanup = async (): Promise<void> => {
    if (closing) return;
    closing = true;
    dashboard.setPhase("closing", "resuming MCU and closing debugger");
    if (demoTimer) clearInterval(demoTimer);
    if (monitor) await monitor.stop();
    renderer.destroy();
  };

  renderer.keyInput.on("keypress", (key: KeyEvent) => {
    if (key.eventType === "release") return;
    if (key.name === "q" || key.name === "escape" || (key.ctrl && key.name === "c")) {
      void cleanup();
    } else if (key.name === "p" && monitor) {
      const paused = monitor.togglePause();
      dashboard.addEvent(paused ? "sampling paused" : "sampling resumed");
    } else if (key.name === "r" && monitor) {
      dashboard.addEvent("debugger reconnect requested");
      void monitor.reconnect();
    }
  });

  if (options.demo) {
    let tick = 0;
    dashboard.setPhase("live", "demo data; no hardware access");
    dashboard.update(createDemoSnapshot(tick));
    demoTimer = setInterval(() => dashboard.update(createDemoSnapshot(++tick)), options.intervalMs);
  } else {
    try {
      const tools = await discoverTools();
      const layout = await loadMonitorLayout(tools.nm);
      monitor = new HardwareMonitor(board, tools, layout, options.intervalMs, {
        onState: (phase, detail) => dashboard.setPhase(phase, detail),
        onSnapshot: (snapshot) => dashboard.update(snapshot),
      });
      monitor.start();
    } catch (error) {
      dashboard.setPhase("error", error instanceof Error ? error.message : String(error));
    }
  }

  await new Promise<void>((resolve) => renderer.once("destroy", resolve));
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
});
