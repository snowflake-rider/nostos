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
  current.transport.protocolStopRequests = previous.transport.protocolStopRequests + 1;
  current.transport.protocolStopAckMatches = previous.transport.protocolStopAckMatches + 1;
  current.transport.protocolStopAckIgnored = previous.transport.protocolStopAckIgnored + 1;
  current.transport.protocolTransmitFailures = previous.transport.protocolTransmitFailures + 1;

  const messages = detectTelemetryEvents(previous, current).map((event) => event.message);
  expect(messages).toContain("BTN1 pressed");
  expect(messages).toContain("Message SPEED_UP");
  expect(messages).toContain("UART TX +1");
  expect(messages).toContain("STOP request RX +1");
  expect(messages).toContain("STOP ACK matched +1");
  expect(messages).toContain("STOP ACK ignored +1");
  expect(messages).toContain("Protocol TX failed +1");
});
