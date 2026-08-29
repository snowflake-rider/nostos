> 이관 원문: `docs/superpowers/specs/2026-08-27-layer-6-symmetric-forwarding-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 6 Symmetric Forwarding Design

Date: 2026-08-27

Status: architecture and specification approved; implementation, host tests,
clean build, and two-board forwarding verified; three-board relay pending.

## 1. Purpose

Layer 6 extends the physically verified Layer 5 custom Advertising packet into
a connectionless, symmetric store-and-forward learning protocol. The same
firmware runs on every ESP32-S3. Every node can originate a packet, receive a
packet, and forward a new logical packet once while TTL permits.

The required three-node evidence is:

```text
A creates packet P
-> B receives P directly
-> B transmits P with TTL decremented
-> C receives the forwarded frame from B
```

Layer 6 remains ordinary BLE Advertising/Scanning with an application-defined
protocol. It is not standard Bluetooth Mesh.

## 2. Scope

Layer 6 preserves:

- one identical ESP-IDF image for all boards
- the Layer 2 GATT Service and RX Write/TX Read/Notify behavior
- connectable Legacy Advertising
- continuous Active Scanning while not stopped by an error
- the Layer 5 20-byte packet and CRC16/CCITT-FALSE rules
- MAC-derived one-byte node identity and `ESP32-L6-XX` device name
- fixed-capacity queues and caches with no per-packet heap allocation

Layer 6 adds:

- a fixed transmit queue shared by origin and forward traffic
- one-hop-at-a-time TTL decrement and CRC regeneration
- deterministic bounded relay jitter
- separate logical-message deduplication and observed-path deduplication
- direct-versus-relayed receive markers
- one-board, two-board, and three-board workflows with different proof limits

Layer 6 does not include:

- standard Bluetooth Mesh Provisioning, keys, Models, or Mesh Relay
- encryption, authentication, pairing, or bonding
- route discovery, route tables, acknowledgements, or delivery guarantees
- persistent Source/Relay/Sink roles
- priority classes, congestion control, or production Company Identifier use
- a claim that the final receiver was outside the origin's radio range

## 3. Symmetric Node Model

All nodes execute these roles concurrently:

```text
GATT Server
+ connectable Advertiser
+ Active Scanner
+ Origin
+ Forwarder
+ Receiver
```

There are no A-specific, B-specific, or C-specific binaries. The triplet
workflow discovers node IDs from runtime logs and accepts any three distinct
nodes that produce a valid `origin -> via -> local` chain.

The Bluetooth public address's final byte is the local node ID. For a received
Advertising report, the final byte of the report address is the immediate
transmitter, called `via`.

## 4. Packet Identity and Forwarding Rules

The Layer 5 wire format remains exactly 20 bytes:

| Byte | Field | Forwarding rule |
|---:|---|---|
| 0 | version | preserve |
| 1 | message type | preserve |
| 2 | TTL | decrement by exactly one |
| 3 | sender | preserve original source |
| 4 | recipient | preserve |
| 5..6 | sequence | preserve |
| 7 | payload length | preserve |
| 8..17 | payload | preserve all ten bytes |
| 18..19 | CRC16 | recompute after TTL change |

`sender + sequence` is the logical message identity. `sender` always means the
origin and never changes to the forwarding node.

An origin packet starts with TTL `2`. A node may forward a valid, new logical
message only when received TTL is greater than zero:

```text
received TTL=2 -> forwarded TTL=1
received TTL=1 -> forwarded TTL=0
received TTL=0 -> receive/log only; no forwarding
```

Sequence zero remains reserved. Local sequences start at 1 and wrap from 65535
to 1. A node records its own `sender + sequence` identity before transmission,
so an over-the-air copy of its own origin packet is never forwarded back into
the network.

Recipient handling remains Layer 5 behavior: accept broadcast `0xFF` or the
local node ID. A packet for another unicast recipient is ignored and is not
forwarded in Layer 6.

## 5. Direct and Relayed Path Classification

Path classification compares the packet origin with the immediate transmitter:

```text
via == packet.sender -> direct frame
via != packet.sender -> relayed frame
```

Example:

```text
packet.sender=A, report address node=A -> direct
packet.sender=A, report address node=B -> relayed through B
```

Logical processing and path observation use separate fixed caches:

- logical cache: 32 FIFO entries keyed by `origin + sequence`
- path cache: 32 FIFO entries keyed by `origin + sequence + via`

The logical cache ensures one application processing/forward decision per
message. The path cache allows one concise log for each distinct transport path.
This separation is required because C may receive A directly before receiving
B's forwarded copy. In that case the forwarded copy is a logical duplicate but
is still valid evidence that B transmitted and C received the relay path.

Expected receive markers:

```text
[LAYER-6] PACKET_RX_DIRECT local=B6 origin=76 via=76 seq=10 ttl=2 duplicate=no crc=ok
[LAYER-6] PACKET_RX_RELAYED local=CC origin=76 via=B6 seq=10 ttl=1 duplicate=yes crc=ok
```

A duplicate path already present in the path cache is counted but not logged
again. A relayed logical duplicate is observed but never forwarded again.

## 6. Receive Pipeline

The GAP callback remains bounded:

1. accept inquiry-result events from non-local Bluetooth addresses
2. locate Manufacturer Specific Data with prototype company ID `0xFFFF`
3. require exactly two company-ID bytes plus 20 packet bytes
4. copy the 20 bytes, RSSI, and report address into the RX queue
5. use zero wait and return

The RX queue capacity remains 16 records. Queue-full events increment a drop
counter without blocking the GAP callback.

The packet worker performs, in order:

1. decode and CRC validation
2. supported version/type and payload-length validation
3. sender, recipient, and immediate-transmitter validation
4. direct/relayed path classification
5. path-cache lookup and bounded path logging
6. logical-cache lookup
7. application receive accounting for a new logical message
8. forwarding decision when TTL is greater than zero
9. bounded relay jitter followed by a zero-wait TX queue submission

Malformed packets and queue overflow produce reason/counter logs. Packet
payload is printed only through a bounded sanitized buffer.

## 7. Relay Jitter and Loop Prevention

Before placing a forward record in the TX queue, the worker waits a deterministic
bounded jitter derived from local node ID, origin, and sequence:

```text
relay_delay_ms = 80 +
    ((local_node_id XOR origin XOR sequence_low XOR sequence_high) modulo 121)

80 ms <= relay_delay_ms <= 200 ms
```

The low Layer 6 traffic rate makes this bounded worker delay acceptable while
keeping the GAP callback unblocked. The delay reduces synchronized
re-advertising but does not guarantee collision avoidance.

Loops are bounded by both mechanisms:

- logical dedup permits only one forward decision per `origin + sequence`
- TTL strictly decreases and TTL zero cannot be forwarded

## 8. Transmit Queue and Advertising State Machine

Layer 5's single periodic packet update becomes a fixed 16-entry TX queue.
Each record contains:

- a complete logical packet structure
- TX kind: `origin` or `forward`
- local immediate-transmitter ID for logging

The first origin packet is the initial raw Advertising frame. After the local
node ID is known, startup waits this deterministic stagger before creating it:

```text
initial_delay_ms = 250 + ((node_id modulo 11) * 100)
```

The node records the initial `sender + sequence` in logical dedup, encodes it
into the raw Advertising buffer, and then registers the GATT application. The
first successful Advertising start logs `PACKET_TX_ORIGIN` for sequence 1.

Later origin generation occurs every 8 seconds measured from the previous
local-origin creation. Sequence increments when the origin record is created,
not when an Advertising update happens. Origin and forwarded packets use the
same queue; records remain FIFO and neither kind has priority in Layer 6.

Only the Advertising state machine accesses the mutable raw Advertising buffer:

```text
TX queue record available
-> wait until not connected and no update is in flight
-> stop current Advertising
-> encode queued packet and CRC into raw Advertising data
-> configure raw Advertising data
-> wait for configuration-complete callback
-> restart connectable Advertising
-> log origin or forward TX after start-complete
-> keep that frame active for at least 500 ms
-> process the next queued record
```

Active Scanning remains enabled throughout an Advertising update. Only one
stop/configure/start transaction may be in flight.

On GATT connection, new Advertising transitions pause. The RX scanner and GATT
Server remain active. TX queue records are retained. On disconnect,
Advertising resumes and queued records continue in FIFO order.

If the TX queue is full, the new record is dropped and a kind-specific counter
increments. There is no overwrite of an older queued record.

Expected TX markers:

```text
[LAYER-6] PACKET_TX_ORIGIN local=76 origin=76 seq=10 ttl=2 payload=HELLO crc=ok
[LAYER-6] PACKET_TX_FORWARD local=B6 origin=76 seq=10 ttl=1 payload=HELLO crc=ok
```

## 9. Public Host-Test Seams

Layer 6 reuses `layer_packet_encode`, `layer_packet_decode`, CRC, and logical
dedup from Layer 5. It adds the following exact public contract in
`layer_relay.c/.h`:

```c
#define LAYER_PATH_DEDUP_CAPACITY 32U

typedef enum {
    LAYER_RELAY_OK = 0,
    LAYER_RELAY_INVALID_ARGUMENT,
    LAYER_RELAY_TTL_EXHAUSTED,
} layer_relay_status_t;

typedef enum {
    LAYER_RELAY_PATH_DIRECT = 0,
    LAYER_RELAY_PATH_RELAYED,
} layer_relay_path_t;

typedef struct {
    uint8_t origin;
    uint16_t sequence;
    uint8_t via;
} layer_path_identity_t;

typedef struct {
    layer_path_identity_t entries[LAYER_PATH_DEDUP_CAPACITY];
    size_t count;
    size_t next;
} layer_path_dedup_t;

layer_relay_status_t layer_relay_prepare_forward(
    const layer_packet_t *received,
    layer_packet_t *forwarded);

layer_relay_path_t layer_relay_classify_path(uint8_t origin, uint8_t via);

void layer_path_dedup_init(layer_path_dedup_t *cache);

bool layer_path_dedup_is_duplicate_or_record(
    layer_path_dedup_t *cache,
    uint8_t origin,
    uint16_t sequence,
    uint8_t via);
```

`layer_relay_prepare_forward` returns `LAYER_RELAY_INVALID_ARGUMENT` for either
null pointer, `LAYER_RELAY_TTL_EXHAUSTED` for TTL zero, and otherwise copies the
entire logical packet, decrements only TTL, and returns `LAYER_RELAY_OK`.
CRC generation remains the responsibility of `layer_packet_encode`.
Firmware-specific FreeRTOS and GAP state remain outside host tests.

Host tests must prove:

- forwarding preserves every field except TTL and later encoded CRC
- TTL 2 becomes 1 and TTL 1 becomes 0
- TTL 0 returns an explicit exhausted result and is not prepared for TX
- the forwarded encoded frame has a valid, changed CRC
- direct classification when `origin == via`
- relayed classification when `origin != via`
- path cache distinguishes the same logical message received through two vias
- repeated `origin + sequence + via` is a path duplicate
- path FIFO eviction remains within 32 entries
- existing Layer 5 codec and logical-dedup tests continue to pass

Development follows RED -> GREEN slices through these public interfaces.

## 10. Runtime Status

Recurring status must expose queue pressure and forwarding state:

```text
[LAYER-6] RELAY_NODE_ACTIVE node=XX gatt=yes advertising=yes scanning=yes connected=no origin_tx=... forward_tx=... rx_new=... rx_relayed_paths=... logical_duplicates=... rx_drops=... tx_drops=...
```

The status line is readiness evidence, not proof of radio reception or relay.

## 11. Repeatable Workflows and Evidence

### One board

`bootload.sh` profiles, builds, flashes, and verifies:

- Layer 6 local startup
- GATT/Advertising/Scanning readiness
- at least one `PACKET_TX_ORIGIN`

It ends with forwarding and triplet reception explicitly not verified.

### Two boards

`bootload-pair.sh` flashes the same image to two boards and verifies:

- two unique node IDs
- both nodes ready
- bilateral origin TX and direct RX
- at least one `PACKET_TX_FORWARD`

This proves forwarding code ran and a board advertised a forwarded frame. It
does not prove a third receiver.

### Three boards

`bootload-triplet.sh` requires exactly three selected ports or exactly three
auto-detected supported USB serial ports. It builds once, flashes the same image
to all three boards, and monitors all ports concurrently for up to 120 seconds.

The script dynamically accepts any chain satisfying all of these:

```text
A, B, and C are three distinct node IDs
A logs PACKET_TX_ORIGIN for identity P
B logs PACKET_RX_DIRECT for P from A
B logs PACKET_TX_FORWARD for P with TTL reduced by one
C logs PACKET_RX_RELAYED for P with origin=A and via=B
all relevant frames report crc=ok
```

The relayed C record may report `duplicate=yes` when C already received A's
direct frame. It still proves C received B's transmitted forwarding frame.

Final triplet summary:

```text
RELAY_CHAIN_ORIGIN=<A>
RELAY_CHAIN_VIA=<B>
RELAY_CHAIN_RECEIVER=<C>
THREE_DISTINCT_NODES=yes
RELAY_PATH_OBSERVED=yes
RESULT=PASS
```

## 12. Verification Stages

The evidence stages remain separate:

1. public host-test RED/GREEN record
2. strict host tests and sanitizer run
3. bash syntax and ShellCheck
4. ESP-IDF clean build
5. one-board flash/runtime/origin TX
6. two-board direct RX/forward TX
7. three-board forwarded-frame reception
8. later RF-isolated necessity test

Only stage 7 completes Layer 6 radio verification. If only two boards are
connected, implementation may be source/build/pair verified but Layer 6 remains
`TRIPLET_RELAY=NOT_VERIFIED`.

## 13. Evidence Boundary

A triplet PASS proves that B received A's logical packet, created a TTL-reduced
frame preserving origin identity, and C received that frame over the air from
B. It does not prove that C was unable to hear A directly.

A stronger necessity test requires a controlled arrangement in which A-to-C
direct reception is absent or blocked while A-to-B and B-to-C remain available,
followed by relay disabled/enabled comparison. That experiment is separate and
must not be inferred from a desk-range triplet PASS.

Layer 6 must always be described as a custom connectionless BLE forwarding
protocol, never as standard Bluetooth Mesh.

## 14. Files

The implementation will create:

```text
layers/layer-6/
  CMakeLists.txt
  sdkconfig.defaults
  README.md
  bootload.sh
  bootload-pair.sh
  bootload-triplet.sh
  main/
    CMakeLists.txt
    main.c
    layer_packet.c
    layer_packet.h
    layer_relay.c
    layer_relay.h
  host-tests/
    run-tests.sh
    test_layer_packet.c
    test_layer_relay.c
  logs/
    README.md
```

Documentation updates occur only after the corresponding verification stage is
actually observed. Layer 5 evidence remains unchanged and independently
reproducible.
