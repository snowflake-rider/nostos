# Protocol and state hygiene assessment

Recommendation: **Strong prerequisite to the ESP32 overhaul**
Category: in-process

## Authoritative path

The active v2 ESP32 path uses `application_message_engine`, whose receiver owns session windows and network state. HELLO and shared-data requests read snapshots from that receiver (`firmware/esp32/main/application_message_engine.c:197-269`; `bridge_runtime_v2.c:422-509`).

## Shadow state to collapse

### `shared_data_cache`

`application_message_engine_accept_wire()` stores RIDE and ENVIRONMENT wires in `engine.cache` (`application_message_engine.c:79-85`), but production never calls `shared_data_cache_copy_fresh()`. The runtime uses receiver snapshots instead. Only `test_shared_data_cache.c` reads the cache.

This creates a second truth with its own session, sequence, freshness, and replacement rules (`shared_data_cache.c:38-128`) but no production leverage.

Deletion test: removing it would not concentrate production complexity; it would remove unread state and a misleading test surface. Collapse it while deepening ESP32 output dispatch.

### `nostos_endpoint`

`nostos_endpoint.*` is compiled into the protocol host library and exercised by `test_nostos_safety.c`, but it has no target caller. The active STM32 v2 runtime uses `sensor_link` and `message_protocol_service`; the active ESP32 v2 runtime uses `application_message_engine`.

Deletion test: removing this test-only endpoint would not spread deployed behavior. Retain only if it is promoted to a real adapter with a production caller; otherwise its tests should move to deployed interfaces.

### `remote_event_backlog`

`remote_event_backlog.c` is absent from production `firmware/esp32/main/CMakeLists.txt` and is referenced only by its dedicated host test. It should either become part of the selected deep dispatch module or be removed as dead interface.

## Deep modules to preserve

- `nostos_state.*` is not shadow state: `application_message_engine` uses its receiver directly. Deleting it would spread session windows, incident state, ordering, freshness, and request policy into the ESP32 runtime.
- `sensor_link.*` is a real cross-target seam. Both STM32 and ESP32 depend on its framing and validation.
- The legacy `event_protocol.*` and `event_bridge.*` remain real because the v1 adapter is still selected when v2 is disabled. Their lifecycle should be made explicit, not silently merged with v2.

## Test assessment

Protocol check passed 4/4, but one test executable mixes deployed core semantics with a non-deployed endpoint. Test names and CMake targets should make the distinction explicit so AI navigation does not treat every passing test as active-runtime proof.
