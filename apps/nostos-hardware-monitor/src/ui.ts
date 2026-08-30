import {
  BoxRenderable,
  TextAttributes,
  TextRenderable,
  type CliRenderer,
} from "@opentui/core";

import {
  AUDIO_STATUS_NAMES,
  RESULT_NAMES,
  enumName,
  messageName,
  type Board,
  type TelemetrySnapshot,
} from "./model.js";

export type ConnectionPhase =
  | "connecting"
  | "live"
  | "paused"
  | "reconnecting"
  | "error"
  | "closing";

function yesNo(value: boolean): string {
  return value ? "YES" : "no";
}

function onOff(value: boolean): string {
  return value ? "ON " : "off";
}

function uartStatus(value: number): string {
  return ["OK", "ERROR", "BUSY", "TIMEOUT"][value] ?? `UNKNOWN(${value})`;
}

function alertState(value: number): string {
  return ["OFF", "EMERGENCY"][value] ?? `UNKNOWN(${value})`;
}

function buzzerPattern(value: number): string {
  return ["NONE", "EMERGENCY"][value] ?? `UNKNOWN(${value})`;
}

function sampleTime(timestamp: number): string {
  return new Date(timestamp).toLocaleTimeString("ko-KR", { hour12: false });
}

function makePanel(
  renderer: CliRenderer,
  title: string,
  options: { width?: number | `${number}%`; height?: number; flexGrow?: number } = {},
): { box: BoxRenderable; text: TextRenderable } {
  const box = new BoxRenderable(renderer, {
    border: true,
    borderStyle: "rounded",
    borderColor: "#334155",
    title,
    titleColor: "#67e8f9",
    padding: 1,
    flexDirection: "column",
    ...options,
  });
  const text = new TextRenderable(renderer, {
    content: "waiting...",
    fg: "#dbeafe",
    width: "100%",
  });
  box.add(text);
  return { box, text };
}

export class Dashboard {
  private readonly connectionText: TextRenderable;
  private readonly buttonText: TextRenderable;
  private readonly rtosText: TextRenderable;
  private readonly outputText: TextRenderable;
  private readonly transportText: TextRenderable;
  private readonly eventText: TextRenderable;
  private readonly footerText: TextRenderable;
  private previous: TelemetrySnapshot | undefined;
  private events: string[] = [];
  private phase: ConnectionPhase = "connecting";
  private detail = "starting st-util and GDB";

  constructor(
    private readonly renderer: CliRenderer,
    private readonly board: Board,
    private readonly intervalMs: number,
  ) {
    const root = new BoxRenderable(renderer, {
      id: "root",
      width: "100%",
      height: "100%",
      flexDirection: "column",
      backgroundColor: "#07111f",
      padding: 1,
      gap: 1,
    });

    const header = new BoxRenderable(renderer, {
      height: 3,
      border: true,
      borderStyle: "double",
      borderColor: "#22d3ee",
      alignItems: "center",
      justifyContent: "center",
    });
    header.add(
      new TextRenderable(renderer, {
        content: "NOSTOS Hardware Monitor",
        fg: "#e0f2fe",
        attributes: TextAttributes.BOLD,
      }),
    );

    const main = new BoxRenderable(renderer, {
      flexDirection: "row",
      flexGrow: 1,
      width: "100%",
      gap: 1,
    });
    const left = new BoxRenderable(renderer, {
      width: "38%",
      minWidth: 38,
      flexDirection: "column",
      gap: 1,
    });
    const right = new BoxRenderable(renderer, {
      flexGrow: 1,
      flexDirection: "column",
      gap: 1,
    });

    const connection = makePanel(renderer, "Connection", { height: 9 });
    this.connectionText = connection.text;
    const buttons = makePanel(renderer, "Buttons (raw / debounced / armed)", { flexGrow: 1 });
    this.buttonText = buttons.text;
    left.add(connection.box);
    left.add(buttons.box);

    const upperRight = new BoxRenderable(renderer, {
      flexDirection: "row",
      height: 11,
      gap: 1,
    });
    const rtos = makePanel(renderer, "FreeRTOS + Queues", { width: "50%" });
    const outputs = makePanel(renderer, "Outputs", { flexGrow: 1 });
    this.rtosText = rtos.text;
    this.outputText = outputs.text;
    upperRight.add(rtos.box);
    upperRight.add(outputs.box);

    const transport = makePanel(renderer, "UART + Protocol v2", { height: 10 });
    this.transportText = transport.text;
    const events = makePanel(renderer, "Live changes", { flexGrow: 1 });
    this.eventText = events.text;
    right.add(upperRight);
    right.add(transport.box);
    right.add(events.box);

    main.add(left);
    main.add(right);

    this.footerText = new TextRenderable(renderer, {
      height: 1,
      content: "q/Esc quit   p pause   r reconnect   SWD attach restarts MCU once",
      fg: "#94a3b8",
      attributes: TextAttributes.DIM,
    });
    root.add(header);
    root.add(main);
    root.add(this.footerText);
    renderer.root.add(root);
    this.renderConnection();
    this.renderEmpty();
  }

  setPhase(phase: ConnectionPhase, detail: string): void {
    this.phase = phase;
    this.detail = detail;
    this.renderConnection();
    this.renderer.requestRender();
  }

  addEvent(message: string): void {
    this.events.unshift(`${sampleTime(Date.now())}  ${message}`);
    this.events = this.events.slice(0, 7);
    this.eventText.content = this.events.join("\n") || "No changes yet";
  }

  update(snapshot: TelemetrySnapshot): void {
    this.detectChanges(snapshot);
    const previous = this.previous;
    const inputDelta = previous
      ? (snapshot.rtos.inputHeartbeat - previous.rtos.inputHeartbeat) >>> 0
      : 0;
    const serviceDelta = previous
      ? (snapshot.rtos.serviceHeartbeat - previous.rtos.serviceHeartbeat) >>> 0
      : 0;

    this.buttonText.content = snapshot.buttons
      .map(
        (button) =>
          `${button.name.padEnd(5)} ${button.pin.padEnd(4)}  ${onOff(button.rawPressed)} / ${onOff(button.stablePressed)} / ${yesNo(button.armed)}`,
      )
      .join("\n");

    this.rtosText.content = [
      `scheduler     ${snapshot.schedulerRunning ? "RUNNING" : "STOPPED"}`,
      `heartbeat     input ${snapshot.rtos.inputHeartbeat} (+${inputDelta})`,
      `              service ${snapshot.rtos.serviceHeartbeat} (+${serviceDelta})`,
      `queues         urgent ${yesNo(snapshot.rtos.urgentQueueReady)}  normal ${yesNo(snapshot.rtos.normalQueueReady)}`,
      `events         queued ${snapshot.rtos.queued}  dispatched ${snapshot.rtos.dispatched}`,
      `errors         full ${snapshot.rtos.queueFull}  expired ${snapshot.rtos.expired}`,
      `BTN4 resets    ${snapshot.rtos.resets}${snapshot.resetPending ? "  PENDING" : ""}`,
    ].join("\n");

    const output = snapshot.outputs;
    this.outputText.content = [
      `audio status   ${enumName(AUDIO_STATUS_NAMES, output.audioStatus)}`,
      `audio stream   ${onOff(output.audioPlaying)} pos=${output.audioPosition}`,
      `RGB actual     R:${onOff(output.rgbRed)} G:${onOff(output.rgbGreen)} B:${onOff(output.rgbBlue)}`,
      `alert state    ${alertState(output.alertState)} led=${onOff(output.alertLedOn)}`,
      `buzzer         ${onOff(output.buzzerActive)} pin=${onOff(output.buzzerPin)}`,
      `pattern        ${buzzerPattern(output.buzzerPattern)}`,
      `last message   ${messageName(snapshot.lastMessage)}`,
    ].join("\n");

    const transport = snapshot.transport;
    this.transportText.content = [
      `v2 boot        ${enumName(RESULT_NAMES, snapshot.protocolStatus)} (${snapshot.protocolStatus})`,
      `UART           ${uartStatus(transport.uartStatus)}  TX ${transport.tx}  RX ${transport.rx}`,
      `UART errors    invalid ${transport.invalid}  dropped ${transport.dropped}`,
      `router         local ${transport.localRouted}  remote ${transport.remoteRouted}`,
      `last RX        ${messageName(transport.lastReceived)}`,
      `protocol RX    ok ${transport.protocolReceived}  dup ${transport.protocolDuplicates}`,
      `protocol err   rejected ${transport.protocolRejected}  overflow ${transport.protocolOverflows}`,
      `last result    ${enumName(RESULT_NAMES, transport.protocolLastResult)}`,
    ].join("\n");

    this.previous = snapshot;
    this.phase = this.phase === "paused" ? "paused" : "live";
    this.detail = `sample ${sampleTime(snapshot.collectedAtMs)}`;
    this.renderConnection();
    this.renderer.requestRender();
  }

  private renderConnection(): void {
    this.connectionText.content = [
      `state     ${this.phase.toUpperCase()}`,
      `detail    ${this.detail}`,
      `node      ${this.board.id}`,
      `ST-Link   ${this.board.serial}`,
      `device    ${this.board.deviceType ?? "STM32"}`,
      `interval  ${this.intervalMs} ms`,
    ].join("\n");
  }

  private renderEmpty(): void {
    this.buttonText.content = "Waiting for first sample...";
    this.rtosText.content = "Waiting for first sample...";
    this.outputText.content = "Waiting for first sample...";
    this.transportText.content = "Waiting for first sample...";
    this.eventText.content = "No changes yet";
  }

  private detectChanges(current: TelemetrySnapshot): void {
    const previous = this.previous;
    if (!previous) {
      this.addEvent(`monitor live; protocol=${enumName(RESULT_NAMES, current.protocolStatus)}`);
      return;
    }

    current.buttons.forEach((button, index) => {
      const old = previous.buttons[index];
      if (old && button.stablePressed !== old.stablePressed) {
        this.addEvent(`${button.name} ${button.stablePressed ? "PRESSED" : "released"}`);
      }
    });
    if (current.lastMessage !== previous.lastMessage && current.lastMessage !== 0) {
      this.addEvent(`message -> ${messageName(current.lastMessage)}`);
    }
    if (current.rtos.dispatched > previous.rtos.dispatched) {
      this.addEvent(`dispatched +${current.rtos.dispatched - previous.rtos.dispatched}`);
    }
    if (current.rtos.resets > previous.rtos.resets) {
      this.addEvent(`BTN4 reset +${current.rtos.resets - previous.rtos.resets}`);
    }
    if (current.transport.tx > previous.transport.tx) {
      this.addEvent(`UART TX +${current.transport.tx - previous.transport.tx}`);
    }
    if (current.outputs.audioPlaying !== previous.outputs.audioPlaying) {
      this.addEvent(`audio ${current.outputs.audioPlaying ? "START" : "stop"}`);
    }
  }
}
