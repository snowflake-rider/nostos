# Layer 7 Standard Bluetooth Mesh Implementation Plan

Date: 2026-08-27
Design: `docs/superpowers/specs/2026-08-27-layer-7-standard-mesh-final-design.md`

## Goal

Create one ESP32-S3 firmware image that exposes Configuration Server, Generic
OnOff Server, Generic OnOff Client, PB-GATT/PB-ADV, GATT Proxy, and standard
Mesh Relay support. An iPhone running nRF Mesh performs Provisioning and
Configuration; any configured board can originate group OnOff through serial
commands.

## Public test seam

The approved host-test seam is the pure-C serial command parser declared by
`serial_command.h`. Tests observe only line-parser input and command enum
output. ESP-BLE-MESH callback behavior is verified by compilation and later
real node logs rather than mocked stack internals.

## Evidence boundary

Source, host tests, clean build, board flash/boot, iPhone discovery,
Provisioning, Configuration, Generic OnOff, and controlled Relay proof remain
separate. Only two serial devices are currently known, so triplet and iPhone
stages stay `NOT_VERIFIED` unless actual new evidence appears.

## Current result

- implementation: complete
- strict parser tests + ASan/UBSan: PASS
- ESP-IDF v5.5.5 ESP32-S3 build: PASS
- two-board flash/boot with distinct identities: PASS
- iPhone Provisioning/Configuration: NOT_VERIFIED
- three-node group OnOff and controlled Relay comparison: NOT_VERIFIED

## Tasks

1. Scaffold `layers/layer-7` as an independent ESP-IDF v5.5.5 ESP32-S3 project.
2. Add the public serial parser header and the first failing `on` parser test.
3. Implement the minimal parser and continue one RED-GREEN slice at a time for
   all approved commands, line endings, unknown input, and overflow recovery.
4. Add a symmetric one-element Mesh Composition with Configuration Server,
   Generic OnOff Server, and Generic OnOff Client.
5. Add MAC-derived node name/UUID, PB-GATT/PB-ADV, GATT Proxy, persistent Mesh
   settings, and Relay-disabled default configuration.
6. Add Provisioning, Configuration Server, Generic Server, and Generic Client
   callbacks with stable Layer 7 markers.
7. Add bounded serial input handling and guarded acknowledged/unacknowledged
   Generic OnOff Client sends using configured publication metadata.
8. Add status and explicit factory-reset behavior.
9. Add one-board and triplet build/flash workflows plus simultaneous triplet
   monitoring without automatic external iPhone mutations.
10. Write the Layer 7 README with exact nRF Mesh steps and evidence boundaries.
11. Run strict host tests, ASan/UBSan, shell checks, stale-marker audit, and
    `idf.py fullclean build`.
12. Detect current hardware and perform only the strongest safe physical stage
    available. Never promote two-board boot to three-board Mesh or Relay proof.
13. Update root roadmap/progress/learning docs with observed evidence only.
