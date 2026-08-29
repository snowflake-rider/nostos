> 이관 원문: `docs/superpowers/specs/2026-08-27-layer-7-standard-mesh-final-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 7 Standard Bluetooth Mesh Final Design

Date: 2026-08-27

Status: architecture approved and implemented. Host tests, ESP-IDF build, and
two-board flash/boot are verified. iPhone Provisioning, three-node group
messaging, and the controlled Relay experiment remain unverified.

## 1. Purpose

Layer 7 jumps from the Layer 6 application-defined Advertising forwarder to a
complete, standard Bluetooth Mesh learning node. The same ESP32-S3 firmware
runs on three boards. An iPhone running nRF Mesh provisions and configures the
nodes. After configuration, any board can issue a Generic OnOff group command
from its serial console and every subscribed board can receive it.

The final physical experiment compares one blocked direct path with the middle
node's standard Mesh Relay feature disabled and enabled.

Layer 7 does not extend or reuse Layer 6's custom packet, CRC, logical dedup,
path dedup, or forwarding state machine. Those responsibilities move to the
ESP-BLE-MESH stack.

## 2. Evidence Boundary

The final implementation keeps these stages separate:

1. source and host-test success
2. clean ESP-IDF build and binary creation
3. flash and boot on each real ESP32-S3
4. iPhone discovery of each unprovisioned UUID
5. Provisioning complete
6. Composition Data and AppKey configuration complete
7. Generic OnOff Client and Server Model Bind complete
8. Server group subscription and Client group publication complete
9. unicast or group Generic OnOff reception
10. Relay OFF/ON comparison with the direct source-to-destination path blocked

No earlier stage implies a later stage. In particular, Provisioning success
does not imply that Composition Data, AppKey, Model Bind, Publication, or
Subscription succeeded.

## 3. Platform and Provisioner

- board target: ESP32-S3 N16R8 boards already used by Layers 0-6
- framework: local ESP-IDF v5.5.5
- Bluetooth host: Bluedroid through ESP-IDF
- Mesh implementation: ESP-BLE-MESH standard stack
- Provisioner/configurator: iPhone nRF Mesh app
- provisioning bearers exposed by the node: PB-GATT and PB-ADV
- expected iPhone provisioning path: PB-GATT
- proxy: GATT Proxy Server enabled
- persistent Mesh settings: enabled in NVS

iOS cannot implement the Mesh Advertising Bearer through CoreBluetooth. The
iPhone therefore needs a GATT Proxy node for normal Mesh messages after
Provisioning. PB-ADV remains enabled on the ESP32-S3 for protocol learning and
compatibility with other Provisioners, but the iPhone path is designed around
PB-GATT and GATT Proxy.

## 4. Alternatives Considered

### Selected: one symmetric Client + Server image

Every board contains Configuration Server, Generic OnOff Server, and Generic
OnOff Client Models. A serial command selects which physical board originates
a message. This preserves one identical image and gives an explicit board
source for node-to-node and Relay testing.

### Rejected for the final design: Server-only nodes with iPhone source

This is simpler firmware, but the source-side GATT Proxy selection makes the
physical Relay topology less explicit.

### Rejected: separate Client and Server images

Roles are simple, but this breaks the existing same-firmware learning goal and
requires multiple build artifacts.

## 5. Project Layout

Layer 7 is an independent ESP-IDF application:

```text
layers/layer-7/
├── CMakeLists.txt
├── sdkconfig.defaults
├── README.md
├── bootload.sh
├── bootload-triplet.sh
├── monitor-triplet.sh
├── host-tests/
│   ├── run-tests.sh
│   └── test_serial_command.c
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    ├── main.c
    ├── mesh_node.c
    ├── mesh_node.h
    ├── serial_command.c
    └── serial_command.h
```

Layer 6 remains unchanged as the custom Advertising forwarding comparison.

## 6. Composition and Mesh Features

Each node has one Element containing:

```text
Element 0
├── Configuration Server
├── Generic OnOff Server
└── Generic OnOff Client
```

Configuration Server defaults:

- default TTL: `7`
- Network Transmit: three transmissions, 20 ms steps through the ESP-IDF macro
- Relay support compiled in
- Relay state at first provisioning: Disabled
- Relay Retransmit: two transmissions, 20 ms steps
- Secure Network Beacon: Enabled
- GATT Proxy: Enabled
- Friend feature: Not Supported unless deliberately enabled later

The middle Relay role is not hard-coded. The Provisioner selects any one node
and sends Config Relay Set. All three boards use the same Composition.

The initial group address is `0xC000`. It is a setup convention, not a
hard-coded destination inside the radio stack. nRF Mesh configures:

- Generic OnOff Server Subscription: `0xC000`
- Generic OnOff Client Publication address: `0xC000`
- the same AppKey bound to both Generic OnOff Models

## 7. Identity and Persistent State

The unprovisioned name is `ESP32-MESH-XX`, where `XX` is the final byte of the
Bluetooth MAC address. The 16-byte Device UUID contains a fixed Layer 7 prefix
plus enough MAC-derived bytes to make the three boards distinct.

ESP-BLE-MESH settings persist Provisioning, keys, Model bindings, publication,
subscription, and Relay configuration in NVS. Application state stores only
the small indices needed to construct Client messages after reboot when those
values are not safely discoverable from restored Model state:

- primary NetKey index
- bound AppKey index
- configured Client publication address

Configuration callbacks update this application metadata. Boot validation
cross-checks stored metadata against the restored Client Model. Invalid or
missing metadata makes `on` and `off` fail with a configuration marker instead
of transmitting with guessed indices.

Flashing a new application does not erase NVS by default. Workflows provide an
explicit `--erase` option for a new Provisioning experiment. The serial
`factory-reset` command calls the Mesh local reset path only after the complete
command is received; it logs the action and reboots into the unprovisioned
state.

## 8. Serial Command Contract

Each board accepts newline-terminated commands at 115200 baud:

| Command | Effect |
|---|---|
| `status` | print Provisioning, address, key, publication, subscription, Relay, and OnOff state |
| `on` | send acknowledged Generic OnOff Set `1` to configured publication address |
| `off` | send acknowledged Generic OnOff Set `0` to configured publication address |
| `on-unack` | send unacknowledged Generic OnOff Set `1` |
| `off-unack` | send unacknowledged Generic OnOff Set `0` |
| `factory-reset` | clear local Mesh state and reboot unprovisioned |

Unknown, overlength, and partial commands do not mutate Mesh state. The parser
is a small pure-C unit with host tests. The firmware uses a bounded line buffer
and never allocates per command.

Every OnOff Set gets a monotonically incrementing transaction identifier. The
client call returning success means that the stack accepted the send request;
it is not logged as remote delivery. A Client callback or a receiving Server
callback supplies later evidence.

## 9. Runtime Data Flow

### Boot and Provisioning

```text
NVS init
-> Bluetooth init
-> MAC-derived name and UUID
-> register Provisioning, Config Server, Generic Server, Generic Client callbacks
-> esp_ble_mesh_init
-> restore settings if present
-> if unprovisioned: enable PB-GATT + PB-ADV
-> accept serial commands
```

### Configuration

```text
iPhone reads Composition Data
-> adds AppKey
-> binds AppKey to Generic OnOff Server
-> binds AppKey to Generic OnOff Client
-> subscribes Server to 0xC000
-> configures Client publication to 0xC000
-> optionally enables Relay on the middle node
```

### Board-originated Generic OnOff

```text
serial `on`
-> validate provisioned + AppKey bound + publication configured
-> build Generic OnOff Set with next TID
-> esp_ble_mesh_generic_client_set_state
-> log request accepted or rejected
-> Generic Client callback logs Status/timeout
```

### Server reception

```text
ESP-BLE-MESH validates network/application security and duplicate state
-> Generic Server callback
-> update OnOff Server state
-> log src, dst, recv_ttl, opcode, tid, and state
-> send Status for acknowledged Set
```

Application code does not manually decrypt, decrement TTL, deduplicate, or
re-advertise the message.

## 10. Required Markers

Markers use a stable `[LAYER-7]` prefix.

Boot and identity:

```text
[LAYER-7] BOOT_SUCCESS target=esp32s3 idf=v5.5.5
[LAYER-7] NODE_IDENTITY node=XX name=ESP32-MESH-XX uuid=...
[LAYER-7] MESH_INITIALIZED
[LAYER-7] UNPROVISIONED_READY bearers=PB-GATT|PB-ADV
```

Provisioning and configuration:

```text
[LAYER-7] PROVISIONING_LINK_OPEN bearer=PB-GATT
[LAYER-7] PROVISIONING_COMPLETE net_idx=.... primary_addr=....
[LAYER-7] APPKEY_ADDED net_idx=.... app_idx=....
[LAYER-7] MODEL_APP_BOUND element=.... model=GEN_ONOFF_SERVER app_idx=....
[LAYER-7] MODEL_APP_BOUND element=.... model=GEN_ONOFF_CLIENT app_idx=....
[LAYER-7] GROUP_SUBSCRIBED model=GEN_ONOFF_SERVER address=C000
[LAYER-7] CLIENT_PUBLICATION_READY address=C000 app_idx=.... ttl=...
[LAYER-7] RELAY_STATE_CHANGED state=enabled retransmit=...
```

Message evidence:

```text
[LAYER-7] ONOFF_TX_REQUEST src=.... dst=C000 state=ON tid=...
[LAYER-7] ONOFF_TX_ACCEPTED dst=C000 state=ON tid=...
[LAYER-7] ONOFF_STATUS_RX src=.... dst=.... state=ON
[LAYER-7] ONOFF_RX src=.... dst=C000 recv_ttl=... state=ON tid=...
[LAYER-7] ONOFF_TX_TIMEOUT dst=C000 opcode=...
```

Failure markers name the failed boundary, including `NOT_PROVISIONED`,
`APPKEY_NOT_BOUND`, `PUBLICATION_NOT_CONFIGURED`, API error codes, and serial
buffer overflow.

## 11. Workflow Design

### `bootload.sh`

- detect/profile one ESP32-S3
- build Layer 7
- optionally erase Flash only when `--erase` is given
- flash and monitor
- require boot, identity, and either restored-provisioned or unprovisioned-ready
- save a timestamped log
- never claim Provisioning or radio success from boot alone

### `bootload-triplet.sh`

- require exactly three explicit or discovered serial ports
- profile all three boards
- build one binary once
- optionally erase all three only with `--erase`
- flash the same artifact to all three
- require three distinct identities and runtime-ready markers
- finish with Provisioning/configuration/message/Relay as `NOT_VERIFIED`

### `monitor-triplet.sh`

- open all three serial ports without resetting them
- prefix each line with physical board label and port
- save one timestamped log
- recognize Provisioning, configuration, TX, RX, and Relay markers
- report each evidence stage separately
- do not infer a Relay hop from Generic OnOff application reception alone

The iPhone operations remain manual because they change external Mesh network
state. The README provides an exact nRF Mesh checklist.

## 12. Test Strategy

### Host tests

- empty command
- `on`, `off`, acknowledged and unacknowledged variants
- `status`
- exact `factory-reset`
- unknown command
- CR, LF, and CRLF handling
- buffer boundary and overflow recovery
- repeated parser commands; TID advance is compiled into the firmware send path

### Build tests

- strict host warnings
- host ASan/UBSan
- shell syntax and ShellCheck
- ESP-IDF `idf.py set-target esp32s3`
- ESP-IDF `idf.py fullclean build`
- binary and partition-size checks

### Physical stages

1. one board flash and unprovisioned advertisement
2. iPhone sees exact name and distinct UUID
3. one board Provisioning and post-Provisioning configuration
4. one-board unicast OnOff
5. three-board Provisioning with distinct unicast addresses
6. group subscription/publication and one source to two receivers
7. reboot persistence
8. blocked-direct-path Relay comparison

## 13. Relay Proof

Standard Mesh application callbacks expose the original source and destination,
not a Layer 6-style immediate `via` field. Therefore a normal `ONOFF_RX` log
cannot identify which Relay retransmitted the network PDU.

The required final evidence is a controlled comparison:

```text
same A, B, C placement
same AppKey, group, TTL, source command, and destination
A-to-C direct path physically blocked or attenuated

B Relay Disabled -> C does not receive across repeated trials
B Relay Enabled  -> C receives across repeated trials
```

A and B must remain mutually reachable, and B and C must remain mutually
reachable. The test records node addresses, Relay state, command TID, receive
TTL, trial count, and results. A close-range three-board group receive proves
Mesh messaging, not range extension or a Relay hop.

## 14. Out of Scope

- Layer 6 custom packet compatibility
- Vendor Model or Bike Swarm Guard warning payload
- STM32 UART integration
- physical LED, buzzer, or safety output
- automatic iPhone UI control
- production OOB authentication, key rotation, IV Update testing, Low Power
  Node/Friend testing, Directed Forwarding, DFU, or road-safety claims

These follow only after unmodified Generic OnOff and the controlled Relay
comparison pass on three real boards.
