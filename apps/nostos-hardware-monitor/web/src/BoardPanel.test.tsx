import { expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";

import { createDemoSnapshot } from "../../src/demo";
import type { WebBoardState } from "../../src/web-contract";
import { BoardPanel } from "./BoardPanel";

test("renders one board with buttons, RTOS, output, and transport telemetry", () => {
  const state: WebBoardState = {
    board: {
      id: "connected-stm32-no-mpu-dht",
      serial: "066DFF485277504867161930",
      firmwareVariant: "node1-base",
    },
    label: "Board 1",
    phase: "live",
    detail: "test fixture",
    snapshot: createDemoSnapshot(1),
    previousSnapshot: createDemoSnapshot(0),
    events: [{ id: "1", at: 1_700_000_000_000, level: "info", message: "BTN1 pressed" }],
    sampleCount: 2,
    droppedSamples: 0,
  };

  const html = renderToStaticMarkup(
    <BoardPanel state={state} reconnecting={false} onReconnect={() => undefined} />,
  );
  expect(html).toContain("Board 1");
  expect(html).toContain("node1-base");
  expect(html).toContain("BTN4");
  expect(html).toContain("FreeRTOS");
  expect(html).toContain("Protocol");
  expect(html).toContain("STOP Requests");
  expect(html).toContain("STOP ACK matched");
  expect(html).toContain("Protocol TX failed");
  expect(html).toContain("BTN1 pressed");
});

test("renders an actionable firmware mismatch without a flash control", () => {
  const state: WebBoardState = {
    board: {
      id: "connected-stm32-2-no-mpu-dht",
      serial: "066EFF3134584B3043121635",
      firmwareVariant: "node1-base",
    },
    label: "Board 2",
    phase: "error",
    detail: "ELF symbol address does not match target flash image (layout version 2 expected)",
    snapshot: undefined,
    previousSnapshot: undefined,
    events: [],
    sampleCount: 0,
    droppedSamples: 0,
  };

  const html = renderToStaticMarkup(
    <BoardPanel state={state} reconnecting={false} onReconnect={() => undefined} />,
  );

  expect(html).toContain("Firmware / ELF mismatch");
  expect(html).toContain("ELF symbol address does not match target flash image");
  expect(html).toContain("Telemetry hidden for safety.");
  expect(html).toContain("This monitor does not flash or modify the board.");
  expect(html).toContain("Reconnect debugger");
  expect(html).not.toMatch(/<button[^>]*>[^<]*Flash/i);
  expect(html).not.toContain("Buttons");
});
