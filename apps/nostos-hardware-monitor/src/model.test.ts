import { describe, expect, test } from "bun:test";

import {
  createMonitorLayout,
  decodeTelemetry,
  parseNmSymbols,
  type MemoryBlock,
} from "./model.js";

function nm(address: number, size: number, name: string): string {
  return `${address.toString(16).padStart(8, "0")} ${size.toString(16).padStart(8, "0")} B ${name}`;
}

function block(address: number, length: number): MemoryBlock {
  return { address, bytes: new Uint8Array(length) };
}

function put8(target: MemoryBlock, address: number, value: number): void {
  target.bytes[address - target.address] = value;
}

function put32(target: MemoryBlock, address: number, value: number): void {
  const offset = address - target.address;
  new DataView(target.bytes.buffer).setUint32(offset, value, true);
}

const SYMBOLS = [
  nm(0x1000, 1, "uart_debug_status"),
  nm(0x1001, 1, "vs1003b_debug_status"),
  nm(0x1002, 1, "protocol_debug_boot_status"),
  nm(0x1010, 80, "buttons"),
  nm(0x1200, 4, "environment_debug_failure_count"),
  nm(0x1204, 4, "message_router_debug_remote_count"),
  nm(0x1208, 4, "message_router_debug_local_count"),
  nm(0x120c, 1, "uart_debug_last_received"),
  nm(0x1210, 4, "uart_debug_dropped_count"),
  nm(0x1214, 4, "uart_debug_invalid_count"),
  nm(0x1218, 4, "uart_debug_rx_count"),
  nm(0x121c, 4, "uart_debug_tx_count"),
  nm(0x1220, 1, "alert_debug_led_on"),
  nm(0x1221, 1, "alert_debug_state"),
  nm(0x1222, 1, "buzzer_debug_pattern"),
  nm(0x1223, 1, "buzzer_debug_active"),
  nm(0x1224, 4, "vs1003b_debug_audio_position"),
  nm(0x1228, 1, "vs1003b_debug_audio_playing"),
  nm(0x1229, 1, "last_message"),
  nm(0x122a, 1, "output_reset_requested"),
  nm(0x2000, 20, "stats"),
  nm(0x2014, 4, "service_heartbeat"),
  nm(0x2018, 4, "input_heartbeat"),
  nm(0x3000, 28, "stats"),
  nm(0x4000, 4, "normal_queue"),
  nm(0x4004, 4, "urgent_queue"),
  nm(0x5000, 4, "xSchedulerRunning"),
].join("\n");

describe("ELF layout and telemetry decoding", () => {
  test("distinguishes duplicate stats symbols and decodes live fields", () => {
    const layout = createMonitorLayout(parseNmSymbols(SYMBOLS));
    expect(layout.appStats.address).toBe(0x2000);
    expect(layout.protocolStats.address).toBe(0x3000);

    const debug = block(layout.debugStart, layout.debugLength);
    const app = block(0x2000, 28);
    const protocol = block(0x3000, 28);
    const queues = block(0x4000, 8);
    const scheduler = block(0x5000, 4);
    const gpioA = block(0x40020010, 8);
    const gpioB = block(0x40020410, 8);
    const gpioC = block(0x40020810, 8);

    put8(debug, 0x1002, 15);
    put8(debug, 0x1010 + 8, 1);
    put8(debug, 0x1010 + 9, 1);
    put8(debug, 0x1010 + 10, 0);
    put32(debug, 0x1010 + 12, 1234);
    put8(debug, 0x1229, 0x11);
    put32(debug, 0x1208, 1);
    put8(debug, 0x1001, 0);

    [3, 0, 0, 2, 1, 4000, 800].forEach((value, index) => {
      put32(app, 0x2000 + index * 4, value);
    });
    put32(queues, 0x4000, 0x20001000);
    put32(queues, 0x4004, 0x20002000);
    put32(protocol, 0x3000, 11);
    put32(protocol, 0x3014, 7);
    put8(protocol, 0x3018, 15);
    put32(scheduler, 0x5000, 1);
    put32(gpioA, 0x40020014, 1 << 4);
    put32(gpioB, 0x40020414, 1 << 0);
    put32(gpioC, 0x40020814, 1 << 1);

    const snapshot = decodeTelemetry(
      layout,
      [debug, app, protocol, queues, scheduler, gpioA, gpioB, gpioC],
      99,
    );
    expect(snapshot.collectedAtMs).toBe(99);
    expect(snapshot.schedulerState).toBe(1);
    expect(snapshot.schedulerRunning).toBe(true);
    expect(snapshot.protocolStatus).toBe(15);
    expect(snapshot.buttons[0]).toMatchObject({
      name: "BTN1",
      rawPressed: true,
      stablePressed: true,
      armed: false,
      changedAtMs: 1234,
    });
    expect(snapshot.rtos).toMatchObject({
      queued: 3,
      dispatched: 2,
      resets: 1,
      serviceHeartbeat: 4000,
      inputHeartbeat: 800,
    });
    expect(snapshot.lastMessage).toBe(0x11);
    expect(snapshot.transport).toMatchObject({
      protocolReceived: 11,
      protocolLastResult: 15,
    });
    expect(snapshot.outputs).toMatchObject({
      rgbRed: true,
      rgbGreen: true,
      rgbBlue: true,
    });
  });
});
