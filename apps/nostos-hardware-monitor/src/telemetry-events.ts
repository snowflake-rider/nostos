import {
  MESSAGE_PROTOCOL_RESULT_NAMES,
  enumName,
  messageName,
  type TelemetrySnapshot,
} from "./model.js";
import type { EventLevel } from "./web-contract.js";

export interface DetectedEvent {
  level: EventLevel;
  message: string;
}

export function detectTelemetryEvents(
  previous: TelemetrySnapshot | undefined,
  current: TelemetrySnapshot,
): DetectedEvent[] {
  if (!previous) {
    return [
      {
        level: current.protocolStatus === 0 ? "info" : "warn",
        message: `Monitor live · Protocol ${enumName(MESSAGE_PROTOCOL_RESULT_NAMES, current.protocolStatus)}`,
      },
    ];
  }

  const events: DetectedEvent[] = [];
  current.buttons.forEach((button, index) => {
    const old = previous.buttons[index];
    if (!old || button.stablePressed === old.stablePressed) return;
    const resetButton = button.name === "BTN4";
    events.push({
      level: resetButton && button.stablePressed ? "warn" : "info",
      message: `${button.name}${resetButton ? " RESET" : ""} ${button.stablePressed ? "pressed" : "released"}`,
    });
  });
  if (current.lastMessage !== previous.lastMessage && current.lastMessage !== 0) {
    events.push({ level: "info", message: `Message ${messageName(current.lastMessage)}` });
  }
  if (current.rtos.dispatched > previous.rtos.dispatched) {
    events.push({
      level: "info",
      message: `Dispatched +${current.rtos.dispatched - previous.rtos.dispatched}`,
    });
  }
  if (current.rtos.queueFull > previous.rtos.queueFull) {
    events.push({
      level: "error",
      message: `Queue full +${current.rtos.queueFull - previous.rtos.queueFull}`,
    });
  }
  if (current.rtos.resets > previous.rtos.resets) {
    events.push({
      level: "warn",
      message: `BTN4 reset +${current.rtos.resets - previous.rtos.resets}`,
    });
  }
  if (current.transport.tx > previous.transport.tx) {
    events.push({
      level: "info",
      message: `UART TX +${current.transport.tx - previous.transport.tx}`,
    });
  }
  if (current.transport.protocolStopRequests > previous.transport.protocolStopRequests) {
    events.push({
      level: "warn",
      message: `STOP request RX +${current.transport.protocolStopRequests - previous.transport.protocolStopRequests}`,
    });
  }
  if (current.transport.protocolStopAckMatches > previous.transport.protocolStopAckMatches) {
    events.push({
      level: "info",
      message: `STOP ACK matched +${current.transport.protocolStopAckMatches - previous.transport.protocolStopAckMatches}`,
    });
  }
  if (current.transport.protocolStopAckIgnored > previous.transport.protocolStopAckIgnored) {
    events.push({
      level: "warn",
      message: `STOP ACK ignored +${current.transport.protocolStopAckIgnored - previous.transport.protocolStopAckIgnored}`,
    });
  }
  if (current.transport.protocolTransmitFailures > previous.transport.protocolTransmitFailures) {
    events.push({
      level: "error",
      message: `Protocol TX failed +${current.transport.protocolTransmitFailures - previous.transport.protocolTransmitFailures}`,
    });
  }
  if (current.outputs.audioPlaying !== previous.outputs.audioPlaying) {
    events.push({
      level: "info",
      message: `Audio ${current.outputs.audioPlaying ? "started" : "stopped"}`,
    });
  }
  return events;
}
