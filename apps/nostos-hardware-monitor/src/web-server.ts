import { join, normalize } from "node:path";

import {
  discoverTools,
  loadBoardFirmware,
  loadBoards,
  REPO_ROOT,
} from "./config.js";
import type { MonitorControl, WebMonitorState } from "./web-contract.js";
import { WebMonitorRuntime } from "./web-runtime.js";

interface Options {
  port: number;
  intervalMs: number;
  demo: boolean;
}

const WEB_DIST = join(REPO_ROOT, "apps/nostos-hardware-monitor/web/dist");

function parseOptions(args: string[]): Options {
  const options: Options = { port: 8787, intervalMs: 250, demo: false };
  for (let index = 0; index < args.length; index += 1) {
    const argument = args[index]!;
    if (argument === "--demo") {
      options.demo = true;
    } else if (argument === "--port") {
      options.port = Number.parseInt(args[++index] ?? "", 10);
    } else if (argument === "--interval") {
      options.intervalMs = Number.parseInt(args[++index] ?? "", 10);
    } else {
      throw new Error(`unknown option: ${argument}`);
    }
  }
  if (!Number.isInteger(options.port) || options.port < 1024 || options.port > 65535) {
    throw new Error("--port must be between 1024 and 65535");
  }
  if (!Number.isInteger(options.intervalMs) || options.intervalMs < 100 || options.intervalMs > 2000) {
    throw new Error("--interval must be between 100 and 2000 ms");
  }
  return options;
}

function json(value: unknown, status = 200): Response {
  return Response.json(value, { status, headers: { "cache-control": "no-store" } });
}

async function main(): Promise<void> {
  const options = parseOptions(Bun.argv.slice(2));
  const boards = options.demo
    ? [
        { id: "demo-node1", serial: "DEMO1", firmwareVariant: "node1-base" as const },
        { id: "demo-node2", serial: "DEMO2", firmwareVariant: "node2-dht11" as const },
        { id: "demo-node3", serial: "DEMO3", firmwareVariant: "node3-mpu6050" as const },
      ]
    : await loadBoards();
  const tools = options.demo ? undefined : await discoverTools();
  const firmwareByBoardId = tools
    ? new Map(
        await Promise.all(
          boards.map(async (board) => [board.id, await loadBoardFirmware(tools, board)] as const),
        ),
      )
    : undefined;
  const runtime = new WebMonitorRuntime({
    boards,
    intervalMs: options.intervalMs,
    demo: options.demo,
    tools,
    firmwareByBoardId,
  });
  const subscribers = new Set<ReadableStreamDefaultController<Uint8Array>>();
  const encoder = new TextEncoder();

  const broadcast = (state: WebMonitorState): void => {
    const chunk = encoder.encode(`data: ${JSON.stringify(state)}\n\n`);
    for (const controller of subscribers) {
      try {
        controller.enqueue(chunk);
      } catch {
        subscribers.delete(controller);
      }
    }
  };
  runtime.subscribe(broadcast);
  const heartbeat = setInterval(() => {
    const chunk = encoder.encode(`: keepalive ${Date.now()}\n\n`);
    for (const controller of subscribers) {
      try {
        controller.enqueue(chunk);
      } catch {
        subscribers.delete(controller);
      }
    }
  }, 2000);

  const server = Bun.serve({
    hostname: "127.0.0.1",
    port: options.port,
    idleTimeout: 255,
    async fetch(request) {
      const url = new URL(request.url);
      if (url.pathname === "/api/state" && request.method === "GET") {
        return json(runtime.getState());
      }
      if (url.pathname === "/api/stream" && request.method === "GET") {
        let controllerRef: ReadableStreamDefaultController<Uint8Array> | undefined;
        const stream = new ReadableStream<Uint8Array>({
          start(controller) {
            controllerRef = controller;
            subscribers.add(controller);
            controller.enqueue(encoder.encode(`retry: 1000\ndata: ${JSON.stringify(runtime.getState())}\n\n`));
          },
          cancel() {
            if (controllerRef) subscribers.delete(controllerRef);
          },
        });
        request.signal.addEventListener("abort", () => {
          if (controllerRef) subscribers.delete(controllerRef);
        });
        return new Response(stream, {
          headers: {
            "content-type": "text/event-stream",
            "cache-control": "no-cache, no-transform",
            connection: "keep-alive",
          },
        });
      }
      if (url.pathname === "/api/control" && request.method === "POST") {
        try {
          const control = (await request.json()) as MonitorControl;
          if (control.action === "pause") runtime.setPaused(control.paused);
          else if (control.action === "interval") runtime.setInterval(control.intervalMs);
          else if (control.action === "reconnect") await runtime.reconnect(control.boardId);
          else return json({ error: "unknown action" }, 400);
          return json(runtime.getState());
        } catch (error) {
          return json({ error: error instanceof Error ? error.message : String(error) }, 400);
        }
      }

      const requested = url.pathname === "/" ? "index.html" : normalize(url.pathname).replace(/^[/\\]+/, "");
      if (requested.includes("..")) return new Response("Not found", { status: 404 });
      const file = Bun.file(join(WEB_DIST, requested));
      if (await file.exists()) return new Response(file);
      const index = Bun.file(join(WEB_DIST, "index.html"));
      return (await index.exists())
        ? new Response(index)
        : new Response("Web build not found. Run: bun run web:build", { status: 503 });
    },
  });

  runtime.start();
  console.log(`NOSTOS Hardware Monitor: http://${server.hostname}:${server.port}`);
  console.log(options.demo ? "Mode: demo (no hardware access)" : `Mode: live (${boards.slice(0, 3).length} boards)`);

  let closing = false;
  const shutdown = async (): Promise<void> => {
    if (closing) return;
    closing = true;
    clearInterval(heartbeat);
    for (const controller of subscribers) controller.close();
    subscribers.clear();
    await runtime.stop();
    server.stop(true);
  };
  process.once("SIGINT", () => void shutdown());
  process.once("SIGTERM", () => void shutdown());
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
});
