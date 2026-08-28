# Layer 6 Symmetric Forwarding Implementation Plan

Date: 2026-08-27
Design: `docs/superpowers/specs/2026-08-27-layer-6-symmetric-forwarding-design.md`

## Goal

Create an independent `layers/layer-6` ESP-IDF application that preserves
Layer 5 GATT and packet behavior, forwards each new logical packet once with
TTL decremented and CRC regenerated, distinguishes direct and relayed paths,
and provides one-board, pair, and triplet verification workflows.

## Evidence boundary

Source, host tests, clean build, flash, pair forwarding, and triplet relay are
separate stages. The current machine has only two detected serial ports, so a
triplet PASS is recorded only if a third actual port becomes available.

## Outcome

Implemented on 2026-08-27. Packet and relay host tests, strict warnings,
ASan/UBSan, shell checks, and an ESP-IDF `fullclean` build passed. The same
firmware was flashed to nodes `76` and `B6`; both exact origin identities were
received directly and forwarded with TTL `2 -> 1`. The pair workflow passed.
Only two serial devices were present, so the required third-node relayed RX is
honestly retained as `TRIPLET_RELAY=NOT_VERIFIED`.

## Tasks

1. Create the Layer 6 project from the verified Layer 5 project without
   modifying Layer 5.
2. Add the approved `layer_relay.h` public contract and one failing host test
   for TTL forwarding.
3. Implement only TTL forwarding until that test passes.
4. Add failing path-classification tests, implement, and return to green.
5. Add failing path-cache identity/FIFO tests, implement, and return to green.
6. Run all inherited packet tests plus relay tests with strict warnings and
   sanitizers.
7. Replace the Layer 5 periodic single-frame update with a 16-entry FIFO TX
   queue and serialized Advertising state machine.
8. Add initial origin staggering, 8-second origin generation, deterministic
   relay jitter, logical dedup, path dedup, and direct/relayed markers.
9. Preserve GATT Server behavior and continuous Active Scanning.
10. Add one-board, pair, and triplet bootload workflows and log naming.
11. Run `bash -n`, ShellCheck, stale-marker audit, host tests, and ESP-IDF
    `fullclean build`.
12. Flash and pair-test the two currently connected boards, checking direct RX
    and forward TX without promoting it to triplet evidence.
13. If three ports are present, run the triplet workflow and require one
    dynamically matched `origin -> via -> local` chain. Otherwise record
    `TRIPLET_RELAY=NOT_VERIFIED`.
14. Update Layer 6 README, roadmap, progress, learning, communication-module,
    and design status only with evidence actually observed.
