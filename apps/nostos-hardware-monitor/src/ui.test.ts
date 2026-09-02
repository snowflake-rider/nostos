import { expect, test } from "bun:test";
import { createTestRenderer } from "@opentui/core/testing";

import { createDemoSnapshot } from "./demo.js";
import { Dashboard } from "./ui.js";

test("dashboard renders the hardware signals and controls", async () => {
  const setup = await createTestRenderer({ width: 120, height: 36 });
  try {
    const dashboard = new Dashboard(
      setup.renderer,
      {
        id: "connected-stm32-no-mpu-dht",
        serial: "066DFF485277504867161930",
        firmwareVariant: "node1-base",
        deviceType: "STM32F411xC_xE",
      },
      250,
    );
    dashboard.setPhase("live", "test fixture");
    dashboard.update(createDemoSnapshot(1));
    await setup.renderOnce();
    const frame = setup.captureCharFrame();

    expect(frame).toContain("NOSTOS Hardware Monitor");
    expect(frame).toContain("firmware  node1-base");
    expect(frame).toContain("Buttons (raw / debounced / armed)");
    expect(frame).toContain("FreeRTOS + Queues");
    expect(frame).toContain("UART + Protocol");
    expect(frame).toContain("NOT_READY (1)");
    expect(frame).toContain("STOP ACK       rx 0  matched 0  ignored 0");
    expect(frame).toContain("protocol TX    total 0  failed 0");
    expect(frame).toContain("q/Esc quit");
  } finally {
    setup.renderer.destroy();
  }
});
