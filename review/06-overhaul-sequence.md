# Recommended overhaul sequence

This is a decision sequence, not an implementation plan. Interface design is deferred until a candidate is selected.

## Phase 0 — protect evidence

1. Keep `firmware/protocol/V2.md` untouched until its current user edit is resolved.
2. Record the active default configurations: ESP32 v2 and STM32 v2 + FreeRTOS.
3. Add composition characterization tests before moving ownership.

Exit criterion: failing tests can demonstrate the current priority, session, retry, and mismatch behavior without hardware.

## Phase 1 — deepen ESP32 output dispatch

Concentrate paired-STM output scheduling and session cleanup first. Remove unread `shared_data_cache` state and decide the fate of `remote_event_backlog` during this work.

Exit criterion: one interface test covers authenticated accept, new-session cleanup, priority, READY gating, snapshot resync, UART failure, and idempotent retry.

## Phase 2 — deepen Mesh delivery

Move retry due time, in-flight state, admission race, TTL, and completion behind one seam while retaining `mesh_node` as the ESP adapter.

Exit criterion: host fault injection covers admission failure, callback-before-return, completion failure, higher-priority replacement, and recovery.

## Phase 3 — deepen STM32 runtime and output application

Make bare-loop and FreeRTOS adapters drive one runtime owner, then place output semantics behind one application seam for legacy and v2 transport.

Exit criterion: the active v2 + FreeRTOS composition is compiled and tested, including reset/calibration ordering and hardware-result reporting.

## Phase 4 — unify verified hardware monitoring

Require the same ELF/flash identity and sampling semantics for TUI and web, then share lifecycle and event interpretation without merging rendering state.

Exit criterion: both adapters fail closed on mismatch and pass the same start/sample/pause/reconnect/close contract tests.

## Phase 5 — deepen release policy and manifests

Do this after firmware module ownership stabilizes, because artifact selection and manifest schema should reflect the final target seams.

Exit criterion: schema-change, package round-trip, tamper, and offline Flash-plan tests pass.

## Verification gates for every phase

- focused host tests first;
- target builds second;
- package and read-back evidence separately;
- physical UART, Mesh, sensor, and output proof separately;
- no Flash, reset, provisioning, or key change without explicit approval.
