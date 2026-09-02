# TODOS

## Firmware Verification

### Prove the STOP path on ESP-IDF and three physical nodes

**What:** Verify one complete STOP transaction across paired UART and Bluetooth Mesh on three provisioned devices. The current ESP32-S3 target already builds with ESP-IDF v5.5.5.

**Why:** Host tests prove portable policy, but they do not execute FreeRTOS queue admission, ESP UART drivers, Mesh callbacks, radio delivery, or the receiving STM32 output path.

**Context:** ESP-IDF v5.5.5 commit `b774170ff46c393eeb5e495ea37936038d3f4f4f` produced the ESP32-S3 application binary on 2026-09-03; this is build evidence only. Capture the chain `STM32 button/fall -> local STOP retry -> paired ESP local ACK -> Mesh STOP -> each peer ESP -> receiving STM32 -> local source-0 ACK -> peer Mesh ACK`. Include UART ACK loss, one full control queue or burst case, BUTTON-to-FALL promotion, a late old ACK, one STM32-only reboot inside and after the 2-second replay window, and a temporary Mesh model-configuration interruption followed by recovery with the same primary address. Confirm `IDENTITY_RESUMED` retains the STOP pending mask and confirm status counters for request/ACK queue overflow and local ACK TX failure. Do not count `api accepted`, host tests, or a successful sender-only log as end-to-end delivery.

**Effort:** M
**Priority:** P1
**Depends on:** ESP-IDF v5.5.5 environment and three provisioned ESP32/STM32 pairs

## Hardware Documentation

### Regenerate KiCad netlist and SVG outputs with Node 1/2/3 labels

**What:** Regenerate committed schematic-derived artifacts after the KiCad source labels changed from Head/Mid/Tail to Node 1/2/3.

**Why:** Stale generated diagrams disagree with the editable schematic and can cause incorrect wiring during assembly.

**Context:** Use a machine with a compatible `kicad-cli`, regenerate only the repository's documented outputs, and review the resulting SVG/netlist diff before committing. Do not hand-edit generated files to imitate the expected labels.

**Effort:** S
**Priority:** P2
**Depends on:** Compatible `kicad-cli` installation

## Completed

- 2026-09-03: Corrected the local inventory example to contain three complete `node1..3` STM32/ESP32 pairs. Firmware and release command examples now use the same node IDs, and the release-tool test suite enforces exactly one STM32 and one ESP32 entry per example node.
- 2026-09-03: Made the STM32 hardware monitor release-variant aware. Each enabled board now derives `node1-base`, `node2-dht11`, or `node3-mpu6050` from its existing `hardwareProfile`, uses that variant's ELF/layout, verifies the complete application image before exposing telemetry, and displays STOP request/ACK/matched/ignored and protocol TX failure counters. Demo mode no longer requires a local board inventory.
- 2026-09-03: Aligned identity documentation and Kconfig with the implemented fail-closed model: Node ID is derived from the provisioned primary address plus one shared `source1..source10` map, not duplicated in application NVS.
- 2026-09-03: Added an `fw check esp32` regression gate for the minimal Mesh composition and required PB-ADV, PB-GATT, Proxy, Relay, Settings, Health Server, GATT client, and BLE scan features.
- 2026-09-03: Reduced the ESP32 Mesh composition to Config Server, Health Server, and one NOSTOS Vendor Model; disabled unused Generic Server, runtime deinit support, and Mesh 1.1 preview code. The ESP32-S3 target binary fell from 920,464 B to 851,120 B without removing PB-ADV, PB-GATT, Proxy, Relay, Settings, or BLE scan.
- 2026-09-03: Added paired-ESP STOP acceptance ACK, 200ms STM32 retry, FALL-priority replacement, a 2-second RAM replay window, and separate STOP request/ACK priority queues.
- 2026-09-03: Replaced ESP32 sensor FIFO backlog with one latest-wins ingress slot per `RIDE_STATE` and `ENVIRONMENT_STATE` Topic while retaining the application runtime's canonical latest state.
- 2026-09-03: Required an exact Complete Local Name match for XOSS scan candidates while retaining GATT CSC service verification.
- 2026-09-03: Added distinct Node 1 base, Node 2 DHT11, and Node 3 MPU6050 STM32 release variants with separate artifacts, receipts, manifest entries, and flash-plan labels.
