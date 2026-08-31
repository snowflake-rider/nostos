# STM32 application runtime assessment

Recommendation: **Strong**
Category: in-process plus ports & adapters

## Current shape

Buttons are polled every 5 ms, calibration edges and reset become notifications, and local messages enter urgent or normal queues. One owner task dispatches at most one event per 1 ms iteration (`firmware/stm32/MyApp/rtos/app_rtos.c:89-230`). V2 RX follows ISR ring → parser → epoch/idempotency checks → output application (`firmware/stm32/MyApp/service/message_protocol_service.c:223-397`).

This concurrency seam is a good foundation. The friction is above it: runtime ownership and output semantics leak across procedural calls and two scheduling adapters.

## Candidate A — deepen application runtime ownership

### Problem

- `app_runtime.h:8-17` exposes seven procedural calls whose implementation is spread through `app.c:139-270`.
- Bare-loop and FreeRTOS paths duplicate calibration-button edge state (`app.c:27-29`, `272-305`; `app_rtos.c:93-119`).
- Ordering, reset, calibration, and application-progression policy therefore lack locality.
- Active defaults are v2 plus FreeRTOS (`firmware/stm32/CMakeLists.txt:29-30`), but host tests compile `app.c` only as v1 and omit `app_rtos.c` (`firmware/stm32/host-tests/CMakeLists.txt:221-229`).

### Deepening direction

Concentrate input normalization, reset and calibration transitions, dispatch ordering, and application progression in one deep application-runtime module. Bare-loop and FreeRTOS become adapters around that seam.

This describes ownership only. The interface shape is intentionally deferred.

### Benefits

- locality: one runtime policy
- leverage: both schedulers
- active composition testable
- reset ordering becomes explicit
- calibration edges stop duplicating

### Deletion test

Deleting the current `app_runtime` calls would mostly inline their implementation into two callers. A deep runtime owner would instead make deletion spread ordering and state policy back across both adapters.

### Test surface

The missing high-value test is the active v2 + FreeRTOS composition, including urgent fairness, expiry, reset ordering, calibration input, and one-event dispatch. The existing 13/13 host result does not cover that composition.

## Candidate B — deepen command/output application

Recommendation: **Strong**

`message_router.c:15-39` is a compile-time switch plus counters. V2 message transport also owns output semantics, idempotency, parser state, UART work, failsafe, and hardware-result reporting (`message_protocol_service.c:105-269`, `338-397`), while legacy output rules live separately in `message_service.c:59-112`.

Concentrate message and FALL output semantics in one deep output-application module. Legacy and v2 transport become adapters. Preserve synchronous `OUTPUT_RESULT` coupling to the actual hardware result.

Deletion test: removing the current router eliminates a near-1:1 interface and moves little hidden implementation. Deleting the proposed output-application module would spread display, audio, alert, and buzzer policy into both transport adapters.

## Candidate C — deepen source-aware sensor state

Recommendation: **Worth exploring**

`sensor_store.c`, `sensor_sync_service.c`, and `sensor_view_service.c` duplicate revision, validity, freshness, and cursor implementation. Display and protocol tests replace adjacent modules with stubs, so no test covers `OUTPUT_* → accepted view → stale → rendered display`.

A deep source-aware sensor-state module should retain the critical distinction between local producer truth and ESP32-accepted presentation while owning provenance, freshness, revision, and projection rules. Do not merge local and displayed truth indiscriminately.

## Candidate D — deepen calibration-session locality

Recommendation: **Worth exploring**

`safety_service.c` already has useful depth, but app and RTOS code still translate its state, track button edges, poll completion for audio, and map presentation intent. Extend depth inside the calibration part of the safety module; keep display and audio as adapters. This should follow the runtime deepening, not precede it.
