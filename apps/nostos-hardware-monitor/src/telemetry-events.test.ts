import { expect, test } from "bun:test";

import { createDemoSnapshot } from "./demo.js";
import { detectTelemetryEvents } from "./telemetry-events.js";

test("detects button and transport changes without replaying old events", () => {
  const previous = createDemoSnapshot(0);
  const current = createDemoSnapshot(1);
  previous.buttons[0]!.stablePressed = false;
  previous.lastMessage = 0;
  current.buttons[0]!.stablePressed = true;
  current.lastMessage = 0x11;
  current.transport.tx = previous.transport.tx + 1;

  const messages = detectTelemetryEvents(previous, current).map((event) => event.message);
  expect(messages).toContain("BTN1 pressed");
  expect(messages).toContain("Message SPEED_UP");
  expect(messages).toContain("UART TX +1");
});
