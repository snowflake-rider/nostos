# ESP32 output dispatch assessment

Recommendation: **Strong — top codebase candidate**
Category: in-process plus ports & adapters

## Current shape

`bridge_runtime_v2.c` owns the protocol bridge, parsers, identity, official writer, application message engine, six sensor-output slots, Mesh retry, Mesh in-flight state, output retry, event heap, and six FreeRTOS queues (`firmware/esp32/main/bridge_runtime_v2.c:58-101`).

An accepted official message crosses the following path:

```text
official wire
  -> application_message_engine
  -> runtime capture and routing
  -> event heap OR sensor slot
  -> runtime priority arbitration
  -> output encode
  -> UART write
  -> exact-command retry
```

Mesh delivery has a second path:

```text
bridge queue
  -> runtime rank comparison
  -> retry slot
  -> in-flight state
  -> mesh_node adapter
  -> asynchronous completion
```

## Candidate A — deepen paired-STM output dispatch

### Problem

- The runtime captures and routes accepted messages itself (`bridge_runtime_v2.c:182-220`).
- Remote-session replacement reaches through `app_engine.receiver.windows` and clears event, retry, and sensor-slot state separately (`bridge_runtime_v2.c:517-570`).
- Sensor-output dispatch and event-output dispatch duplicate decode, encode, UART, statistics, and retry implementation (`bridge_runtime_v2.c:715-795`, `864-937`).
- READY and priority decisions inspect internal counts across several modules (`bridge_runtime_v2.c:735-749`, `806-817`).
- `application_message_engine.h`, `application_event_heap.h`, and `output_command_retry.h` expose most of their implementation state, so their interfaces are shallow (`application_message_engine.h:23-30`, `application_event_heap.h:15-28`, `output_command_retry.h:9-18`).

### Deepening direction

Concentrate acceptance materialization, event and sensor scheduling, READY gating, exact command retry, and remote-session invalidation in one deep output-dispatch module. Keep FreeRTOS and UART as adapters. Keep `official_packet_writer` separate because it already has useful depth.

This describes ownership only. The interface shape is intentionally deferred.

### Benefits

- locality: one accepted-message path
- leverage: every output type
- tests hit one interface
- session cleanup becomes atomic
- retry policy stops leaking

### Deletion test

Deleting the current heap and retry modules would mostly inline their exposed state transitions into `bridge_runtime_v2.c`; complexity would move, not concentrate. Deleting the proposed output-dispatch module would force capture, priority, session, TTL, and idempotency policy back into the runtime adapter. That is the depth target.

### Test surface

`test_nostos_project_scenario.c` reconstructs the production order by hand, while host CMake never compiles `bridge_runtime_v2.c`. The current 13/13 result proves module invariants, not the deployed choreography.

The first surviving test surface should cover:

- authenticated accept through output decision;
- new-session cleanup across every pending class;
- FALL/STOP/button/sensor priority interaction;
- UART partial-failure retry with the same command id;
- READY false/true transitions;
- snapshot resynchronization after HELLO.

## Candidate B — deepen Mesh delivery

Recommendation: **Strong**

### Problem

- Mesh state is split among `mesh_retry`, an external due-time variable, `mesh_inflight`, `nostos_bridge`, and the runtime (`bridge_runtime_v2.c:71-74`, `682-687`).
- Priority is repeated in the retry module, runtime rank helpers, and direct bridge-count reads (`bridge_runtime_v2.c:806-825`, `1004-1015`).
- Admission and callback race handling spans `start_mesh_send`, `process_one`, and the Mesh completion callback (`bridge_runtime_v2.c:940-1067`, `1265-1279`).
- The retry and in-flight modules expose all state behind operation-heavy interfaces. They are shallow despite being individually testable.

### Deepening direction

Concentrate queue selection, retained retry, due time, in-flight identity, admission race, TTL, and completion in one deep Mesh-delivery module. `mesh_node` remains the hardware adapter; a host fault-injection adapter would make this a real seam.

### Deletion test

Deleting `mesh_retry` or `mesh_inflight` today only moves their implementation into the runtime. Deleting the proposed delivery module would concentrate all asynchronous delivery policy in the hardware adapter, confirming the intended depth.

## Candidate C — deepen boot identity and session ownership

Recommendation: **Worth exploring**

Durable session advance, Mesh-primary binding, writer rebinding, READY announcement, HELLO resync, and repeated identity gates are dispersed through `bridge_runtime_v2.c:250-459`. The host boot test compiles the writer, not the NVS/binding/READY lifecycle. A deep boot/session module would give those invariants locality while keeping NVS and Mesh identity as adapters.

Before pursuing this candidate, decide whether `xoss_ble_reset_runtime_session()` is dead interface or a missing lifecycle call; it currently has no production caller.
