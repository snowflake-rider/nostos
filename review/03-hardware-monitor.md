# Hardware monitor assessment

Recommendation: **Strong**
Category: ports & adapters

TUI and web are two real adapters, so their shared monitoring semantics justify a seam. Current duplication has already produced safety drift.

## Candidate A — deepen verified telemetry acquisition

### Problem

- `MonitorLayout` exposes raw symbols and partial ranges (`apps/nostos-hardware-monitor/src/model.ts:15-22`, `155-200`).
- `DebugSession.sample()` reconstructs additional ranges and hard-codes peripheral blocks (`debug-session.ts:316-335`).
- Decoding repeats symbol names, structure offsets, and GPIO addresses (`model.ts:203-287`).
- Web loads an expected flash prefix and passes it into the runtime (`web-server.ts:50-63`, `web-runtime.ts:86-98`).
- TUI constructs `HardwareMonitor` without that value (`index.ts:124-131`), and `DebugSession` skips verification when it is absent (`debug-session.ts:287-290`).

### Deepening direction

Concentrate ELF validation, sampling plan, flash identity, memory reads, and decoding in one deep verified-telemetry module. TUI and web should consume the same verified snapshot seam.

This describes ownership only. The interface shape is intentionally deferred.

### Benefits

- locality: one memory contract
- leverage: both adapters
- mismatch fails closed everywhere
- symbols and offsets stop leaking
- lifecycle tests gain one seam

### Deletion test

Deleting the current `MonitorLayout` abstraction would merely move raw symbol complexity into `DebugSession` and the decoder. Deleting the proposed verified-telemetry module would spread ELF, flash, sampling, and decoding rules across both adapters.

### Test surface

Current tests cover one synthetic layout/decode path and byte-array mismatch logic. Missing tests include start → verify → sample → resume → close, missing symbols, read failure, and TUI/web verification parity.

Preserve the existing depth in `DebugSession`, especially its GDB/`st-util` handling and resume-in-`finally` behavior.

## Candidate B — deepen shared monitor state and interpretation

Recommendation: **Strong**

### Problem

- TUI owns demo timing, callbacks, cleanup, pause, and reconnect in `index.ts:92-135`.
- Web repeats live/demo selection, pause, interval, reconnect, phase, event, and snapshot state in `web-runtime.ts:71-183`.
- Web uses `detectTelemetryEvents()` (`telemetry-events.ts:9-65`), while TUI maintains a second event implementation (`ui.ts:268-296`). Web reports queue-full events; TUI does not.
- Phase state is repeated in monitor callbacks, the web contract, and TUI state.

### Deepening direction

Concentrate source selection, lifecycle, phase transitions, snapshot history, and domain-event interpretation in one deep monitor-state module. Keep rendering and input inside the TUI and web adapters.

### Deletion test

Deleting `telemetry-events.ts` only moves its implementation into `web-runtime.ts`, because TUI already has another copy. A deep shared state module would make deletion spread lifecycle and event policy back into both adapters.

### Constraint

Single-board TUI and three-board web have different presentation needs. Share monitoring semantics, not rendering state.
