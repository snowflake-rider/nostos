export type Stm32FirmwareVariant =
  | "node1-base"
  | "node2-dht11"
  | "node3-mpu6050";

export interface Board {
  id: string;
  serial: string;
  firmwareVariant: Stm32FirmwareVariant;
  chipId?: string;
  deviceType?: string;
}

export interface SymbolInfo {
  address: number;
  size: number;
  type: string;
  name: string;
}

export interface MonitorLayout {
  symbols: Map<string, SymbolInfo[]>;
  appStats: SymbolInfo;
  protocolStats: SymbolInfo;
  buttons: SymbolInfo;
  debugStart: number;
  debugLength: number;
}

export interface MemoryBlock {
  address: number;
  bytes: Uint8Array;
}

export interface ButtonSnapshot {
  name: string;
  pin: string;
  rawPressed: boolean;
  stablePressed: boolean;
  armed: boolean;
  changedAtMs: number;
}

export interface TelemetrySnapshot {
  collectedAtMs: number;
  schedulerState: number;
  schedulerRunning: boolean;
  protocolStatus: number;
  buttons: ButtonSnapshot[];
  resetPending: boolean;
  lastMessage: number;
  rtos: {
    queued: number;
    queueFull: number;
    expired: number;
    dispatched: number;
    resets: number;
    inputHeartbeat: number;
    serviceHeartbeat: number;
    urgentQueueReady: boolean;
    normalQueueReady: boolean;
  };
  transport: {
    uartStatus: number;
    tx: number;
    rx: number;
    invalid: number;
    dropped: number;
    localRouted: number;
    protocolReceived: number;
    protocolDuplicates: number;
    protocolRejected: number;
    protocolOverflows: number;
    protocolStateUpdates: number;
    protocolPaceRequests: number;
    protocolStopRequests: number;
    protocolStopAcks: number;
    protocolStopAckMatches: number;
    protocolStopAckIgnored: number;
    protocolTransmitted: number;
    protocolTransmitFailures: number;
    protocolLastResult: number;
  };
  outputs: {
    audioStatus: number;
    audioPlaying: boolean;
    audioPosition: number;
    buzzerActive: boolean;
    buzzerPattern: number;
    alertState: number;
    alertLedOn: boolean;
    rgbRed: boolean;
    rgbGreen: boolean;
    rgbBlue: boolean;
    buzzerPin: boolean;
  };
}

const BUTTON_META = [
  ["BTN1", "PB5"],
  ["BTN2", "PB10"],
  ["BTN3", "PA8"],
  ["BTN4", "PC7"],
  ["TEST", "PB6"],
] as const;

// message_protocol_stats_t in firmware/stm32/MyApp/service/message_protocol_service.h.
// The monitor deliberately rejects a different size instead of guessing an ABI.
const PROTOCOL_STATS_LAYOUT = {
  size: 52,
  received: 0,
  rejected: 4,
  duplicates: 8,
  overflows: 12,
  stateUpdates: 16,
  paceRequests: 20,
  stopRequests: 24,
  stopAcks: 28,
  stopAckMatches: 32,
  stopAckIgnored: 36,
  transmitted: 40,
  transmitFailures: 44,
  lastResult: 48,
} as const;

export class MemoryImage {
  constructor(private readonly blocks: MemoryBlock[]) {}

  private byteAt(address: number): number {
    for (const block of this.blocks) {
      const offset = address - block.address;
      if (offset >= 0 && offset < block.bytes.length) {
        return block.bytes[offset] ?? 0;
      }
    }
    throw new Error(`memory address 0x${address.toString(16)} was not sampled`);
  }

  u8(address: number): number {
    return this.byteAt(address);
  }

  u16(address: number): number {
    return this.byteAt(address) | (this.byteAt(address + 1) << 8);
  }

  u32(address: number): number {
    return (
      this.byteAt(address) |
      (this.byteAt(address + 1) << 8) |
      (this.byteAt(address + 2) << 16) |
      (this.byteAt(address + 3) << 24)
    ) >>> 0;
  }
}

export function parseNmSymbols(output: string): Map<string, SymbolInfo[]> {
  const result = new Map<string, SymbolInfo[]>();
  for (const line of output.split(/\r?\n/)) {
    const match = line.match(/^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+(\S)\s+(.+)$/);
    if (!match) continue;
    const info: SymbolInfo = {
      address: Number.parseInt(match[1]!, 16),
      size: Number.parseInt(match[2]!, 16),
      type: match[3]!,
      name: match[4]!,
    };
    const entries = result.get(info.name) ?? [];
    entries.push(info);
    result.set(info.name, entries);
  }
  return result;
}

export function requireUniqueSymbol(
  symbols: Map<string, SymbolInfo[]>,
  name: string,
): SymbolInfo {
  const entries = symbols.get(name) ?? [];
  if (entries.length !== 1) {
    throw new Error(`expected one ELF symbol ${name}, found ${entries.length}`);
  }
  return entries[0]!;
}

export function createMonitorLayout(symbols: Map<string, SymbolInfo[]>): MonitorLayout {
  const buttons = requireUniqueSymbol(symbols, "buttons");
  const serviceHeartbeat = requireUniqueSymbol(symbols, "service_heartbeat");
  const stats = symbols.get("stats") ?? [];
  const appStats = stats.find(
    (entry) => entry.size === 20 && entry.address + entry.size === serviceHeartbeat.address,
  );
  const protocolStatsCandidates = stats.filter(
    (entry) => entry !== appStats && entry.size === PROTOCOL_STATS_LAYOUT.size,
  );
  const protocolStats = protocolStatsCandidates.length === 1
    ? protocolStatsCandidates[0]
    : undefined;
  if (!appStats) {
    throw new Error("could not identify the 20-byte app stats symbol");
  }
  if (!protocolStats) {
    throw new Error(
      `expected exactly one ${PROTOCOL_STATS_LAYOUT.size}-byte message protocol stats symbol`,
    );
  }

  const debugNames = [
    "uart_debug_status",
    "vs1003b_debug_status",
    "protocol_debug_boot_status",
    "buttons",
    "environment_debug_failure_count",
    "message_router_debug_local_count",
    "uart_debug_dropped_count",
    "uart_debug_invalid_count",
    "uart_debug_rx_count",
    "uart_debug_tx_count",
    "alert_debug_led_on",
    "alert_debug_state",
    "buzzer_debug_pattern",
    "buzzer_debug_active",
    "vs1003b_debug_audio_position",
    "vs1003b_debug_audio_playing",
    "last_message",
    "output_reset_requested",
  ];
  const debugSymbols = debugNames.map((name) => requireUniqueSymbol(symbols, name));
  const debugStart = Math.min(...debugSymbols.map((entry) => entry.address));
  const debugEnd = Math.max(...debugSymbols.map((entry) => entry.address + entry.size));

  return {
    symbols,
    appStats,
    protocolStats,
    buttons,
    debugStart,
    debugLength: debugEnd - debugStart,
  };
}

function address(layout: MonitorLayout, name: string): number {
  return requireUniqueSymbol(layout.symbols, name).address;
}

export function decodeTelemetry(
  layout: MonitorLayout,
  blocks: MemoryBlock[],
  collectedAtMs = Date.now(),
): TelemetrySnapshot {
  const memory = new MemoryImage(blocks);
  const buttonStride = layout.buttons.size / BUTTON_META.length;
  if (!Number.isInteger(buttonStride) || buttonStride < 16) {
    throw new Error(`unexpected button state layout size ${layout.buttons.size}`);
  }

  const buttons = BUTTON_META.map(([name, pin], index) => {
    const base = layout.buttons.address + index * buttonStride;
    return {
      name,
      pin,
      rawPressed: memory.u8(base + 8) !== 0,
      stablePressed: memory.u8(base + 9) !== 0,
      armed: memory.u8(base + 10) !== 0,
      changedAtMs: memory.u32(base + 12),
    };
  });

  const appStats = layout.appStats.address;
  const protocolStats = layout.protocolStats.address;
  const gpioA = 0x40020000;
  const gpioB = 0x40020400;
  const gpioC = 0x40020800;
  const gpioAOutput = memory.u32(gpioA + 0x14);
  const gpioBOutput = memory.u32(gpioB + 0x14);
  const gpioCOutput = memory.u32(gpioC + 0x14);
  const schedulerState = memory.u32(address(layout, "xSchedulerRunning"));

  return {
    collectedAtMs,
    schedulerState,
    schedulerRunning: schedulerState !== 0,
    protocolStatus: memory.u8(address(layout, "protocol_debug_boot_status")),
    buttons,
    resetPending: memory.u8(address(layout, "output_reset_requested")) !== 0,
    lastMessage: memory.u8(address(layout, "last_message")),
    rtos: {
      queued: memory.u32(appStats),
      queueFull: memory.u32(appStats + 4),
      expired: memory.u32(appStats + 8),
      dispatched: memory.u32(appStats + 12),
      resets: memory.u32(appStats + 16),
      serviceHeartbeat: memory.u32(address(layout, "service_heartbeat")),
      inputHeartbeat: memory.u32(address(layout, "input_heartbeat")),
      normalQueueReady: memory.u32(address(layout, "normal_queue")) !== 0,
      urgentQueueReady: memory.u32(address(layout, "urgent_queue")) !== 0,
    },
    transport: {
      uartStatus: memory.u8(address(layout, "uart_debug_status")),
      tx: memory.u32(address(layout, "uart_debug_tx_count")),
      rx: memory.u32(address(layout, "uart_debug_rx_count")),
      invalid: memory.u32(address(layout, "uart_debug_invalid_count")),
      dropped: memory.u32(address(layout, "uart_debug_dropped_count")),
      localRouted: memory.u32(address(layout, "message_router_debug_local_count")),
      protocolReceived: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.received),
      protocolRejected: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.rejected),
      protocolDuplicates: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.duplicates),
      protocolOverflows: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.overflows),
      protocolStateUpdates: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.stateUpdates),
      protocolPaceRequests: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.paceRequests),
      protocolStopRequests: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.stopRequests),
      protocolStopAcks: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.stopAcks),
      protocolStopAckMatches: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.stopAckMatches),
      protocolStopAckIgnored: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.stopAckIgnored),
      protocolTransmitted: memory.u32(protocolStats + PROTOCOL_STATS_LAYOUT.transmitted),
      protocolTransmitFailures: memory.u32(
        protocolStats + PROTOCOL_STATS_LAYOUT.transmitFailures,
      ),
      protocolLastResult: memory.u8(protocolStats + PROTOCOL_STATS_LAYOUT.lastResult),
    },
    outputs: {
      audioStatus: memory.u8(address(layout, "vs1003b_debug_status")),
      audioPlaying: memory.u8(address(layout, "vs1003b_debug_audio_playing")) !== 0,
      audioPosition: memory.u32(address(layout, "vs1003b_debug_audio_position")),
      buzzerActive: memory.u8(address(layout, "buzzer_debug_active")) !== 0,
      buzzerPattern: memory.u8(address(layout, "buzzer_debug_pattern")),
      alertState: memory.u8(address(layout, "alert_debug_state")),
      alertLedOn: memory.u8(address(layout, "alert_debug_led_on")) !== 0,
      rgbRed: (gpioAOutput & (1 << 4)) !== 0,
      rgbGreen: (gpioBOutput & (1 << 0)) !== 0,
      rgbBlue: (gpioCOutput & (1 << 1)) !== 0,
      buzzerPin: (gpioBOutput & (1 << 4)) !== 0,
    },
  };
}

export const MESSAGE_NAMES: Record<number, string> = {
  0x00: "NONE",
  0x10: "SPEED_DOWN",
  0x11: "SPEED_UP",
  0x13: "STOP",
  0x30: "FALL",
  0xff: "UNKNOWN",
};

export const NOSTOS_RESULT_NAMES = [
  "OK",
  "EMPTY",
  "BAD_ARGUMENT",
  "BAD_LENGTH",
  "BAD_VALUE",
  "TOO_LARGE",
  "UNSUPPORTED_TYPE",
  "BAD_CRC",
  "TIMEOUT",
] as const;

export const MESSAGE_PROTOCOL_RESULT_NAMES = [
  "OK",
  "NOT_READY",
  "BAD_ARGUMENT",
  "BAD_VALUE",
  "IO_ERROR",
] as const;

export const AUDIO_STATUS_NAMES = [
  "OK",
  "INVALID_ARGUMENT",
  "DREQ_TIMEOUT",
  "SPI_ERROR",
  "MODE_MISMATCH",
  "REGISTER_MISMATCH",
  "BUSY",
] as const;

export function enumName(values: readonly string[], value: number): string {
  return values[value] ?? `UNKNOWN(${value})`;
}

export function messageName(value: number): string {
  return MESSAGE_NAMES[value] ?? `0x${value.toString(16).padStart(2, "0")}`;
}
