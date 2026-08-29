> 이관 원문: `docs/superpowers/specs/2026-08-27-layer-4-5-dual-role-packet-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 Layer 4/5 Dual Role and Custom Packet Design

Date: 2026-08-27

Implementation status: Layer 4 and Layer 5 complete on two ESP32-S3 boards.
Layer 5 host tests, clean build, two-board flash, bilateral `PACKET_TX`, and
bidirectional CRC-valid `PACKET_RX_NEW` passed. Evidence:
`layers/layer-5/logs/esp32s3-layer-5-packet-node-pair-20260827T150742-KST.log`.

## 1. Purpose

Layer 4 and Layer 5 extend the verified ESP32-S3 BLE learning sequence without
claiming relay or Bluetooth Mesh behavior.

- Layer 4 proves that two ESP32-S3 boards running the same firmware can each
  provide the Layer 2 GATT Server, connectable Advertising, and Active Scanning
  at the same time, and can discover one another over the air.
- Layer 5 preserves all Layer 4 behavior and proves bidirectional exchange of a
  fixed 20-byte application packet with CRC validation and duplicate detection.

Each Layer remains an independent, complete ESP-IDF project. Flashing a later
Layer replaces the previous application; the Layers are not installed on top of
one another.

## 2. Explicit Scope Boundary

Layer 4 includes:

- BLE-only Bluedroid initialization on ESP32-S3
- the Layer 2 GATT Service and RX Write/TX Read/Notify characteristics
- connectable Legacy Advertising
- Active Scanning
- a MAC-derived node identity and device name
- bidirectional discovery between two boards running the same Layer 4 image

Layer 5 adds:

- a fixed 20-byte packet format
- one automatic `HELLO` broadcast per second while not GATT-connected
- CRC-16/CCITT-FALSE encode and validation
- a fixed 32-entry duplicate cache keyed by sender and sequence
- queued receive processing outside the GAP callback
- bidirectional packet exchange between two boards running the same Layer 5 image

Neither Layer includes:

- forwarding or retransmitting a received packet
- decrementing TTL during forwarding
- a three-board relay claim
- Bluetooth Mesh, Provisioning, AppKey, Models, or the Mesh Relay Feature
- pairing, bonding, or application encryption
- persistent node-role configuration

## 3. Shared Symmetric Firmware Architecture

Both boards receive the same binary. No A-specific or B-specific build exists.

After Bluetooth initialization, the firmware reads the board's Bluetooth MAC
address. The final MAC byte becomes the one-byte learning `node_id`. The device
name includes the Layer and two uppercase hexadecimal digits:

```text
ESP32-L4-76
ESP32-L4-B6

ESP32-L5-76
ESP32-L5-B6
```

The one-byte identity is sufficient for this two-board learning exercise. A
collision check is part of the pair workflow: two boards reporting the same
`node_id` cause failure. A larger network would require a wider identity and is
outside this design.

Every board has three concurrent BLE roles:

```text
GATT Server + connectable Advertiser + Active Scanner
```

The scanner observes peers but does not initiate a GATT connection. A phone may
connect to the GATT Server. While connected, that board stops Advertising as in
Layer 2, continues Scanning and serving GATT, then resumes Advertising after
disconnect.

## 4. Stable GATT Interface

Layer 4 and Layer 5 retain the Layer 2 128-bit Service, RX, and TX UUIDs:

```text
Service  7A110000-6B0D-4D5A-8F4B-2C9E00000001
RX       7A110000-6B0D-4D5A-8F4B-2C9E00000002
TX       7A110000-6B0D-4D5A-8F4B-2C9E00000003
```

The phone-facing behavior also remains stable:

```text
Phone RX Write("HELLO")
-> ESP32 TX value becomes "ACK:HELLO"
-> TX Read returns the ACK
-> TX Notify sends the ACK when notifications are enabled
```

Prepared Writes and zero-length or oversized values remain rejected. A GATT
disconnect requests Advertising restart without stopping the scanner.

## 5. Layer 4 Radio Data Flow

Layer 4 uses connectable Legacy Advertising. The primary Advertising data and
scan response contain:

```text
Primary Advertising
- standard BLE flags

Scan Response
- complete local name: ESP32-L4-XX
- complete 128-bit Layer Service UUID
```

The name AD structure consumes 13 bytes and the Service UUID AD structure
consumes 18 bytes, exactly filling the 31-byte Legacy scan-response limit.

Active Scanning is required because the identifying name and Service UUID are
in the scan response. A scan result is a Layer 4 peer only when all conditions
hold:

- complete local name has the exact `ESP32-L4-XX` shape
- complete or partial 128-bit Service UUID list contains the Layer Service UUID
- the advertised Bluetooth address is not the local Bluetooth address
- the parsed peer node ID differs from the local node ID

The first valid peer result and periodic later results produce:

```text
[LAYER-4] PEER_FOUND name=ESP32-L4-B6 node=B6 rssi=-41 peer=...
[LAYER-4] PEER_RX node=B6 count=1 rssi=-41
```

A recurring heartbeat makes late monitor attachment verifiable:

```text
[LAYER-4] DUAL_ROLE_ACTIVE gatt=yes advertising=yes scanning=yes peer_count=24
```

## 6. Layer 5 Packet Contract

The packet is exactly 20 bytes:

| Byte | Field | Size | Rule |
|---:|---|---:|---|
| 0 | version | 1 | `1` |
| 1 | message_type | 1 | `1` means `HELLO` |
| 2 | ttl | 1 | initially `2`; no decrement in Layer 5 |
| 3 | sender | 1 | local MAC-derived node ID |
| 4 | recipient | 1 | `0xFF` broadcast |
| 5-6 | sequence | 2 | unsigned little-endian counter |
| 7 | payload_length | 1 | `0..10`; Layer 5 sends `5` |
| 8-17 | payload | 10 | `HELLO` followed by zero padding |
| 18-19 | crc16 | 2 | CRC stored little-endian |

CRC-16/CCITT-FALSE uses polynomial `0x1021`, initial value `0xFFFF`, no reflected
input/output, and no final XOR. It covers bytes 0 through 17.

Sequence starts at 1 after boot. Sequence zero is skipped after wrap so the next
value after 65535 is 1. `sender + sequence` identifies one logical message.

TTL is the number of remaining forwarding hops, not seconds. Layer 5 carries and
logs TTL but does not relay, decrement, or retransmit received packets.

## 7. Layer 5 Advertising Layout and Update State Machine

Layer 5 uses raw Legacy Advertising data so byte placement is explicit:

```text
Primary Advertising: 27 bytes
- Flags AD structure: 3 bytes
- Manufacturer Specific Data AD structure: 24 bytes
  - prototype company identifier 0xFFFF: 2 bytes
  - Layer 5 packet: 20 bytes

Scan Response: 31 bytes
- complete local name ESP32-L5-XX: 13 bytes
- complete 128-bit Layer Service UUID: 18 bytes
```

The prototype company identifier is intentionally reserved for local learning
and must not be represented as an assigned production Company Identifier.

When the board is not GATT-connected, a FreeRTOS task requests one new packet
per second. Advertising data is not mutated in place. GAP events drive this
state machine:

```text
one-second request
-> stop current Advertising
-> encode next packet and CRC
-> configure raw Advertising data
-> wait for configuration-complete event
-> restart connectable Advertising
```

Active Scanning remains enabled through an Advertising update. Only one update
may be in flight. An update requested while one is pending increments a skipped
update counter instead of starting overlapping GAP operations.

On GATT connection, new Advertising updates pause. Scanning and the GATT Server
continue. On disconnect, the next packet is encoded and Advertising resumes.

## 8. Layer 5 Receive Pipeline

The GAP scan callback performs only bounded extraction:

1. confirm the `ESP32-L5-XX` name and Layer Service UUID
2. locate Manufacturer Specific Data with prototype ID `0xFFFF`
3. confirm exactly 20 packet bytes are present
4. copy packet bytes, RSSI, and peer address into a fixed FreeRTOS queue
5. return without CRC or duplicate-cache processing

The queue holds 16 receive records. `xQueueSend` uses zero wait in the callback.
If full, the packet is dropped and `RX_QUEUE_DROP` count increases.

A packet worker task validates:

- version is 1
- message type is known
- payload length is at most 10
- recipient is local node ID or `0xFF`
- sender is not the local node ID
- CRC matches

Malformed packets are rejected with a bounded reason log. Payload bytes are
printed through a sanitized fixed-size buffer, never treated as an unbounded C
string.

The duplicate cache is a fixed 32-entry FIFO replacement array. It stores
`sender + sequence`; it is not LRU and allocates no heap memory. A valid unseen
packet produces:

```text
[LAYER-5] PACKET_RX_NEW sender=76 sequence=42 ttl=2 crc=ok payload=HELLO rssi=-41
```

Repeated Advertising events for that logical packet produce a duplicate count.
Duplicate logs are rate-limited to prevent serial flooding.

## 9. Runtime State and Error Handling

Firmware initialization failures use `ESP_ERROR_CHECK` when continued operation
would be invalid. Asynchronous GAP/GATT failures log an exact marker and leave
the corresponding readiness flag false.

Layer 4 recurring status:

```text
[LAYER-4] DUAL_ROLE_ACTIVE gatt=yes advertising=yes scanning=yes peer_count=...
```

Layer 5 recurring status:

```text
[LAYER-5] PACKET_NODE_ACTIVE gatt=yes advertising=yes scanning=yes tx=... rx_new=... duplicate=... crc_fail=... queue_drop=...
```

The heartbeat reports `advertising=no` while a phone is connected or during the
brief packet-update state. Pair verification therefore requires an observed
successful Advertising start and recurring scan/peer evidence, rather than
requiring both boards' instantaneous heartbeat to say `advertising=yes` in the
same millisecond.

## 10. Project and Source Layout

```text
layers/layer-4/
  CMakeLists.txt
  sdkconfig.defaults
  README.md
  bootload.sh
  bootload-pair.sh
  main/CMakeLists.txt
  main/main.c
  logs/README.md

layers/layer-5/
  CMakeLists.txt
  sdkconfig.defaults
  README.md
  bootload.sh
  bootload-pair.sh
  main/CMakeLists.txt
  main/main.c
  main/layer_packet.c
  main/layer_packet.h
  host-tests/test_layer_packet.c
  host-tests/run-tests.sh
  logs/README.md
```

`layer_packet.c` is platform-independent C. It owns packet encode/decode, CRC,
and the fixed duplicate cache. It does not include ESP-IDF or FreeRTOS headers,
which permits strict host compilation.

## 11. One-Board and Pair Workflows

Each project has two workflows:

- `bootload.sh` profiles, builds, flashes, and verifies that one board reaches
  that Layer's local runtime-ready state. It does not claim peer reception.
- `bootload-pair.sh` is the physical two-board proof. It builds once, profiles
  and flashes two distinct ports sequentially, then monitors both ports together.

Both scripts accept explicit ports. The pair script also auto-selects when
exactly two supported serial ports exist:

```bash
./bootload-pair.sh --port-a /dev/cu.usbmodemXXXX \
                   --port-b /dev/cu.usbmodemYYYY
```

The pair workflow rejects:

- missing or more than two auto-detected ports
- equal A and B port paths
- non-existent ports
- a device that does not profile as ESP32-S3 with 16 MB flash
- two boards that report the same node ID
- build or flash failure
- timeout before both boards produce required radio evidence

It writes one combined log with `[A]` and `[B]` prefixes for serial records.

Layer 4 pair PASS requires, for both A and B:

- GATT Server ready
- successful Advertising start
- successful Active Scanning start
- `PEER_RX` naming the other node

Final Layer 4 summary:

```text
PAIR_ADV_SCAN=PASS
RESULT=PASS
```

Layer 5 pair PASS requires all Layer 4 readiness evidence plus, for both boards:

- at least one `PACKET_TX`
- at least one valid `PACKET_RX_NEW` from the other node
- received packet reports `crc=ok`

Final Layer 5 summary:

```text
BIDIRECTIONAL_PACKET_RX_NEW=yes
PAIR_PACKET_TX=PASS
PAIR_PACKET_RX=PASS
PAIR_PACKET_CRC=PASS
RESULT=PASS
```

Pair success proves bidirectional direct radio exchange. It does not prove a
third board, packet forwarding, or relay behavior.

## 12. Verification Plan

Static verification for both Layers:

- `bash -n` for each workflow script
- ShellCheck for each workflow script
- placeholder and stale-Layer marker audit outside build and log directories
- ESP-IDF clean build for ESP32-S3
- generated configuration check for 16 MB flash, Bluedroid, GATT Server,
  Legacy Advertising, and Legacy Scanning

Layer 5 host tests compile with strict warnings and validate:

- packet structure and encoded output are exactly 20 bytes
- a fixed vector has the expected CRC
- encode/decode round trip
- one-byte corruption causes CRC rejection
- payload length greater than 10 is rejected
- the first sender/sequence pair is new
- the same sender/sequence pair is duplicate
- a new sequence is accepted
- FIFO replacement remains within the 32-entry fixed capacity

Hardware verification uses two ESP32-S3 boards connected to the Mac at once.
Layer 4 is flashed and pair-verified first. Layer 5 then replaces Layer 4 on both
boards and is independently pair-verified. Build, flash, local runtime, peer
discovery, and packet reception remain distinct recorded stages.

## 13. Log Names

```text
layers/layer-4/logs/
  esp32s3-layer-4-dual-role-YYYYMMDDTHHMMSS-KST.log
  esp32s3-layer-4-dual-role-pair-YYYYMMDDTHHMMSS-KST.log

layers/layer-5/logs/
  esp32s3-layer-5-packet-node-YYYYMMDDTHHMMSS-KST.log
  esp32s3-layer-5-packet-node-pair-YYYYMMDDTHHMMSS-KST.log
```

Failure logs retain the last stage, non-zero exit code, selected port or ports,
and the exact missing marker or hardware-profile reason.

## 14. Completion Boundary

Layer 4 is complete only after source and clean build pass and two actual boards
both advertise, scan, and log receipt of the other board.

Layer 5 is complete only after protocol host tests and clean build pass and two
actual boards both transmit and receive a CRC-valid new packet from one another.

If only one board is physically connected, implementation and build may be
completed but pair radio status remains unverified. No result is promoted to
Relay or Bluetooth Mesh success.
