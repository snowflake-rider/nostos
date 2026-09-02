import { expect, test } from "bun:test";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { FIRMWARE_ROOT, loadBoards, stm32ElfPath } from "./config.js";

test("maps each STM32 hardware profile to its release ELF", async () => {
  const directory = await mkdtemp(join(tmpdir(), "nostos-monitor-config-"));
  const inventoryPath = join(directory, "boards.local.json");
  try {
    await writeFile(
      inventoryPath,
      JSON.stringify({
        schemaVersion: 1,
        devices: [
          {
            id: "node1",
            target: "stm32",
            transport: "stlink",
            serial: "STLINK1",
            hardwareProfile: {
              ssd1306Display: true,
              mpu6050Sensor: false,
              dht11Sensor: false,
            },
            enabled: true,
          },
          {
            id: "node2",
            target: "stm32",
            transport: "stlink",
            serial: "STLINK2",
            hardwareProfile: {
              ssd1306Display: true,
              mpu6050Sensor: false,
              dht11Sensor: true,
            },
            enabled: true,
          },
          {
            id: "node3",
            target: "stm32",
            transport: "stlink",
            serial: "STLINK3",
            hardwareProfile: {
              ssd1306Display: true,
              mpu6050Sensor: true,
              dht11Sensor: false,
            },
            enabled: true,
          },
        ],
      }),
    );

    const boards = await loadBoards(inventoryPath);
    expect(boards.map((board) => board.firmwareVariant)).toEqual([
      "node1-base",
      "node2-dht11",
      "node3-mpu6050",
    ]);
    const release = await Bun.file(join(FIRMWARE_ROOT, "profiles/release.json")).json() as {
      targets: { stm32: { variants: Array<{ id: string; buildDirectory: string }> } };
    };
    for (const board of boards) {
      const variant = release.targets.stm32.variants.find(
        (candidate) => candidate.id === board.firmwareVariant,
      );
      expect(variant).toBeDefined();
      expect(stm32ElfPath(board.firmwareVariant)).toBe(
        join(FIRMWARE_ROOT, variant!.buildDirectory, "nostos_stm32.elf"),
      );
    }
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("rejects an STM32 hardware profile without a release variant", async () => {
  const directory = await mkdtemp(join(tmpdir(), "nostos-monitor-config-"));
  const inventoryPath = join(directory, "boards.local.json");
  try {
    await writeFile(
      inventoryPath,
      JSON.stringify({
        schemaVersion: 1,
        devices: [
          {
            id: "unsupported",
            target: "stm32",
            transport: "stlink",
            serial: "STLINK4",
            hardwareProfile: {
              ssd1306Display: true,
              mpu6050Sensor: true,
              dht11Sensor: true,
            },
            enabled: true,
          },
        ],
      }),
    );

    await expect(loadBoards(inventoryPath)).rejects.toThrow(
      "STM32 hardwareProfile does not match a release variant",
    );
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
