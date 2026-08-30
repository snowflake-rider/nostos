import type { TelemetrySnapshot } from "./model.js";

export function createDemoSnapshot(tick: number): TelemetrySnapshot {
  const pressed = Math.floor(tick / 5) % 5;
  const active = tick % 5 < 2;
  const message = [0x11, 0x10, 0x13, 0x00, 0x13][pressed] ?? 0;
  return {
    collectedAtMs: Date.now(),
    schedulerState: 1,
    schedulerRunning: true,
    protocolStatus: 15,
    buttons: ["BTN1", "BTN2", "BTN3", "BTN4", "TEST"].map((name, index) => ({
      name,
      pin: ["PB5", "PB10", "PA8", "PC7", "PB6"][index] ?? "?",
      rawPressed: index === pressed && active,
      stablePressed: index === pressed && active,
      armed: !(index === pressed && active),
      changedAtMs: tick * 250,
    })),
    resetPending: false,
    lastMessage: message,
    rtos: {
      queued: tick,
      queueFull: 0,
      expired: 0,
      dispatched: tick,
      resets: pressed === 3 ? Math.floor(tick / 25) : 0,
      inputHeartbeat: tick * 50,
      serviceHeartbeat: tick * 250,
      urgentQueueReady: true,
      normalQueueReady: true,
    },
    transport: {
      uartStatus: 1,
      tx: 0,
      rx: 0,
      invalid: 0,
      dropped: 0,
      lastReceived: 0,
      localRouted: tick,
      remoteRouted: 0,
      protocolReceived: 0,
      protocolDuplicates: 0,
      protocolRejected: 0,
      protocolOverflows: 0,
      protocolLastResult: 15,
    },
    outputs: {
      audioStatus: 0,
      audioPlaying: false,
      audioPosition: 0,
      buzzerActive: false,
      buzzerPattern: 0,
      alertState: 0,
      alertLedOn: false,
      rgbRed: false,
      rgbGreen: false,
      rgbBlue: false,
      buzzerPin: false,
    },
  };
}
