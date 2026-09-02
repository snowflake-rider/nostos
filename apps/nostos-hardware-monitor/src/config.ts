import { dirname, join, resolve } from "node:path";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";

import {
  createMonitorLayout,
  parseNmSymbols,
  type Board,
  type MonitorLayout,
  type Stm32FirmwareVariant,
} from "./model.js";

export const REPO_ROOT = resolve(import.meta.dir, "../../..");
export const FIRMWARE_ROOT = join(REPO_ROOT, "firmware");
export const INVENTORY_PATH = join(FIRMWARE_ROOT, "inventory/boards.local.json");
export const INVENTORY_EXAMPLE_PATH = join(
  FIRMWARE_ROOT,
  "inventory/boards.example.json",
);
export const STM32_ELF_PATH = join(
  FIRMWARE_ROOT,
  "stm32/build/Release/nostos_stm32.elf",
);
const STM32_FLASH_CAPACITY = 512 * 1024;

interface Stm32HardwareProfile {
  ssd1306Display: boolean;
  mpu6050Sensor: boolean;
  dht11Sensor: boolean;
}

const STM32_VARIANTS: Record<
  Stm32FirmwareVariant,
  { buildDirectory: string; hardwareProfile: Stm32HardwareProfile }
> = {
  "node1-base": {
    buildDirectory: "Release",
    hardwareProfile: {
      ssd1306Display: true,
      mpu6050Sensor: false,
      dht11Sensor: false,
    },
  },
  "node2-dht11": {
    buildDirectory: "Release-node2-dht11",
    hardwareProfile: {
      ssd1306Display: true,
      mpu6050Sensor: false,
      dht11Sensor: true,
    },
  },
  "node3-mpu6050": {
    buildDirectory: "Release-node3-mpu6050",
    hardwareProfile: {
      ssd1306Display: true,
      mpu6050Sensor: true,
      dht11Sensor: false,
    },
  },
};

interface InventoryDevice {
  id?: unknown;
  target?: unknown;
  transport?: unknown;
  serial?: unknown;
  chipId?: unknown;
  deviceType?: unknown;
  hardwareProfile?: unknown;
  enabled?: unknown;
}

interface InventoryDocument {
  schemaVersion?: unknown;
  devices?: unknown;
}

export interface ToolPaths {
  stUtil: string;
  gdb: string;
  nm: string;
  objcopy?: string;
}

export interface BoardFirmware {
  elfPath: string;
  layout: MonitorLayout;
  expectedFlashImage: Uint8Array;
}

function stm32FirmwareVariant(value: unknown): Stm32FirmwareVariant {
  if (!value || typeof value !== "object") {
    throw new Error("enabled STM32 inventory entry is missing hardwareProfile");
  }
  const raw = value as Record<string, unknown>;
  const profile: Stm32HardwareProfile = {
    ssd1306Display: raw.ssd1306Display as boolean,
    mpu6050Sensor: raw.mpu6050Sensor as boolean,
    dht11Sensor: raw.dht11Sensor as boolean,
  };
  if (Object.values(profile).some((field) => typeof field !== "boolean")) {
    throw new Error("enabled STM32 hardwareProfile fields must be boolean");
  }
  const match = Object.entries(STM32_VARIANTS).find(([, candidate]) =>
    Object.entries(candidate.hardwareProfile).every(
      ([name, expected]) => profile[name as keyof Stm32HardwareProfile] === expected,
    ),
  );
  if (!match) {
    throw new Error("STM32 hardwareProfile does not match a release variant");
  }
  return match[0] as Stm32FirmwareVariant;
}

export function stm32ElfPath(variant: Stm32FirmwareVariant): string {
  return join(
    FIRMWARE_ROOT,
    "stm32/build",
    STM32_VARIANTS[variant].buildDirectory,
    "nostos_stm32.elf",
  );
}

export async function loadBoards(path = INVENTORY_PATH): Promise<Board[]> {
  const inventory = Bun.file(path);
  if (!(await inventory.exists())) {
    throw new Error(
      `board inventory not found: ${path}; copy ${INVENTORY_EXAMPLE_PATH} and enter the real ST-Link serials`,
    );
  }
  const source = await inventory.text();
  const document = JSON.parse(source) as InventoryDocument;
  if (document.schemaVersion !== 1 || !Array.isArray(document.devices)) {
    throw new Error(`invalid board inventory: ${path}`);
  }

  const boards: Board[] = [];
  for (const raw of document.devices as InventoryDevice[]) {
    if (
      raw.target !== "stm32" ||
      raw.transport !== "stlink" ||
      raw.enabled !== true
    ) {
      continue;
    }
    if (typeof raw.id !== "string" || typeof raw.serial !== "string") {
      throw new Error("enabled STM32 inventory entry is missing id or serial");
    }
    boards.push({
      id: raw.id,
      serial: raw.serial,
      firmwareVariant: stm32FirmwareVariant(raw.hardwareProfile),
      chipId: typeof raw.chipId === "string" ? raw.chipId : undefined,
      deviceType: typeof raw.deviceType === "string" ? raw.deviceType : undefined,
    });
  }

  if (boards.length === 0) {
    throw new Error("no enabled STM32 boards are registered");
  }
  const ids = new Set(boards.map((board) => board.id));
  if (ids.size !== boards.length) {
    throw new Error("STM32 inventory contains duplicate board IDs");
  }
  const serials = new Set(boards.map((board) => board.serial));
  if (serials.size !== boards.length) {
    throw new Error("STM32 inventory contains duplicate ST-Link serials");
  }
  return boards;
}

function which(name: string): string | undefined {
  const result = Bun.spawnSync(["/usr/bin/env", "which", name], {
    stdout: "pipe",
    stderr: "ignore",
  });
  if (result.exitCode !== 0) return undefined;
  const path = new TextDecoder().decode(result.stdout).trim();
  return path.length > 0 ? path : undefined;
}

async function toolBesideCachedCompiler(name: string): Promise<string | undefined> {
  const compilerDefinition = join(
    FIRMWARE_ROOT,
    "stm32/build/Release/CMakeFiles/3.31.6/CMakeCCompiler.cmake",
  );
  const candidates: string[] = [];
  if (await Bun.file(compilerDefinition).exists()) candidates.push(compilerDefinition);

  const glob = new Bun.Glob("**/CMakeCCompiler.cmake");
  for await (const relative of glob.scan({
    cwd: join(FIRMWARE_ROOT, "stm32/build/Release/CMakeFiles"),
    onlyFiles: true,
  })) {
    candidates.push(join(FIRMWARE_ROOT, "stm32/build/Release/CMakeFiles", relative));
  }

  for (const candidate of candidates) {
    const source = await Bun.file(candidate).text();
    const match = source.match(/set\(CMAKE_C_COMPILER "([^"]*arm-none-eabi-gcc)"\)/);
    if (!match) continue;
    const tool = join(dirname(match[1]!), name);
    if (await Bun.file(tool).exists()) return tool;
  }
  return undefined;
}

export async function discoverTools(): Promise<ToolPaths> {
  const stUtil = process.env.NOSTOS_ST_UTIL ?? which("st-util");
  const gdb =
    process.env.NOSTOS_ARM_GDB ??
    which("arm-none-eabi-gdb") ??
    (await toolBesideCachedCompiler("arm-none-eabi-gdb"));
  const nm =
    process.env.NOSTOS_ARM_NM ??
    which("arm-none-eabi-nm") ??
    (await toolBesideCachedCompiler("arm-none-eabi-nm"));
  const objcopy =
    process.env.NOSTOS_ARM_OBJCOPY ??
    which("arm-none-eabi-objcopy") ??
    (await toolBesideCachedCompiler("arm-none-eabi-objcopy"));

  if (!stUtil) throw new Error("st-util not found; install stlink or set NOSTOS_ST_UTIL");
  if (!gdb) throw new Error("arm-none-eabi-gdb not found; set NOSTOS_ARM_GDB");
  if (!nm) throw new Error("arm-none-eabi-nm not found; set NOSTOS_ARM_NM");
  return { stUtil, gdb, nm, objcopy };
}

export async function loadFirmwareImage(
  objcopy: string | undefined,
  elfPath = STM32_ELF_PATH,
): Promise<Uint8Array> {
  if (!objcopy) {
    throw new Error(
      "arm-none-eabi-objcopy not found; set NOSTOS_ARM_OBJCOPY for firmware verification",
    );
  }
  if (!(await Bun.file(elfPath).exists())) {
    throw new Error(
      `STM32 ELF not found: ${elfPath}; run firmware/tools/fw release-build stm32`,
    );
  }

  const temporaryDirectory = await mkdtemp(join(tmpdir(), "nostos-monitor-"));
  const binaryPath = join(temporaryDirectory, "nostos_stm32.bin");
  try {
    const result = Bun.spawnSync(
      [objcopy, "-O", "binary", elfPath, binaryPath],
      { stdout: "pipe", stderr: "pipe" },
    );
    if (result.exitCode !== 0) {
      const detail = new TextDecoder().decode(result.stderr).trim();
      throw new Error(`arm-none-eabi-objcopy failed${detail ? `: ${detail}` : ""}`);
    }
    const binary = new Uint8Array(await Bun.file(binaryPath).arrayBuffer());
    if (binary.length === 0) throw new Error("STM32 ELF produced an empty binary image");
    if (binary.length > STM32_FLASH_CAPACITY) {
      throw new Error(
        `STM32 ELF image is ${binary.length} bytes, larger than 512 KB Flash`,
      );
    }
    return binary;
  } finally {
    await rm(temporaryDirectory, { recursive: true, force: true });
  }
}

export async function loadMonitorLayout(
  nm: string,
  elfPath = STM32_ELF_PATH,
): Promise<MonitorLayout> {
  if (!(await Bun.file(elfPath).exists())) {
    throw new Error(
      `STM32 ELF not found: ${elfPath}; run firmware/tools/fw release-build stm32`,
    );
  }
  const result = Bun.spawnSync([nm, "-S", "-n", elfPath], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (result.exitCode !== 0) {
    throw new Error(`arm-none-eabi-nm failed: ${new TextDecoder().decode(result.stderr)}`);
  }
  return createMonitorLayout(parseNmSymbols(new TextDecoder().decode(result.stdout)));
}

export async function loadBoardFirmware(
  tools: ToolPaths,
  board: Board,
): Promise<BoardFirmware> {
  const elfPath = stm32ElfPath(board.firmwareVariant);
  const [layout, expectedFlashImage] = await Promise.all([
    loadMonitorLayout(tools.nm, elfPath),
    loadFirmwareImage(tools.objcopy, elfPath),
  ]);
  return { elfPath, layout, expectedFlashImage };
}
