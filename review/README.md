# NOSTOS architecture review

Date: 2026-08-31
Scope: architecture assessment only; no implementation interface is proposed here.

## Outcome

The codebase has several genuinely deep modules, but the active v2 runtime composition is not deep enough. The highest-friction path is ESP32 output dispatch: message acceptance, session replacement, priority, sensor snapshots, UART retry, and Mesh delivery are split across exposed state and orchestration in a 1,430-line runtime. The current host tests validate the pieces but reconstruct the production choreography instead of testing it through one interface.

## Ranked candidates

| Rank | Candidate | Strength | Why now |
| ---: | --- | --- | --- |
| 1 | [Deepen ESP32 output dispatch](01-esp32-output-dispatch.md) | Strong | Latest hot spot; priority and retry policy leak across several seams. |
| 2 | [Deepen STM32 application runtime](02-stm32-application-runtime.md) | Strong | The default v2 + FreeRTOS composition is outside the host test interface. |
| 3 | [Deepen verified hardware telemetry](03-hardware-monitor.md) | Strong | TUI and web are two real adapters, but only web requires flash identity. |
| 4 | [Collapse shadow protocol state](04-protocol-state-hygiene.md) | Strong | Test-only or unread state creates false confidence and weak AI locality. |
| 5 | [Deepen release policy and manifest handling](05-release-pipeline.md) | Worth exploring | Security-sensitive schema knowledge leaks through raw dictionaries. |

The proposed order and exit criteria are in [06-overhaul-sequence.md](06-overhaul-sequence.md).

## Scope evidence

- Recent history is concentrated in STM32 application flow, ESP32 v2 runtime, shared protocol/state, release tooling, and display/output tests.
- `CONTEXT.md` is absent, so this review uses the domain terms already present in code: official message, sensor link, output command, Mesh delivery, calibration, sensor state, and hardware telemetry.
- `docs/adr/` is absent, so no candidate is known to contradict an ADR.
- Existing user work in `firmware/protocol/V2.md` was read only and left untouched.

## Verification performed

| Check | Result | What it proves |
| --- | --- | --- |
| `bash firmware/tools/fw check protocol` | 4/4 passed | Shared protocol modules compile and pass fast host tests. |
| `bash firmware/tools/fw check stm32` | 13/13 passed | STM32 host test modules pass. |
| `bash firmware/tools/fw check esp32` | 13/13 passed | ESP32 host test modules pass. |
| Hardware monitor `bun test` | 8 passed | Current TypeScript unit tests pass. |
| `python3 -m unittest firmware.tools.test_release` | 11 passed | Current release-tool unit tests pass. |

No target build, full sanitizer run, Flash, reset, provisioning, Mesh-key change, or physical UART/Mesh/sensor/output test was performed. Passing checks are not E2E proof.

## Positive depth to preserve

- `sensor_link.*`: deleting it would spread framing, CRC, validation, and resynchronization into both firmware targets. Its interface is broad because the wire contract is broad, but it has high leverage.
- `xoss_ble.*`: a small interface hides a large BLE implementation; this is a deep module.
- `official_packet_writer.*`: message identity and incident construction have good locality.
- `DebugSession`: it already hides substantial GDB and `st-util` implementation and resumes execution in a `finally` path.
- Release path and symlink checks: these are security-sensitive implementation that should stay behind the release seam.
