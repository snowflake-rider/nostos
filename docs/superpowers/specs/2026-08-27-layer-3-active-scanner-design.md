> 이관 원문: `docs/superpowers/specs/2026-08-27-layer-3-active-scanner-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 3: BLE Active Scanner Design

Date: 2026-08-27
Target: ESP32-S3 N16R8
SDK: ESP-IDF v5.5.5, Bluedroid host

## Goal

Layer 3 proves direct, connectionless BLE reception between two physical
ESP32 boards:

```text
Board A: Layer 2 advertiser
  -> ESP32-LAYER-2 Advertising + scan response
  -> over-the-air BLE packet
Board B: Layer 3 active scanner
  -> target name, Service UUID, RSSI, and receive count
```

Layer 3 is an independent ESP-IDF project under `layers/layer-3`. It does not
modify Layer 0, Layer 1, or Layer 2. Board C remains unused for this Layer.

## Hardware Roles

- Board A stays powered from an independent USB supply and runs Layer 2.
- Board A must not be connected to a phone because Layer 2 stops Advertising
  while its single GATT connection is active.
- Board B is the only board connected to the Mac data USB port and receives
  Layer 3 firmware.
- Board C stays unplugged so it cannot be selected accidentally.

The observed Board B serial path before implementation is
`/dev/cu.usbmodem1401`; its USB product is `USB JTAG_serial debug unit` and
its USB serial number is `44:1B:F6:FF:BA:B4`. The bootloading workflow must
still re-profile the live device instead of trusting this recorded path.

## Scope

Layer 3 includes:

- BLE Controller and Bluedroid initialization;
- GAP active-scanning parameters;
- continuous scanning without initiating a connection;
- safe parsing of combined Advertising and scan-response data;
- matching the Layer 2 name `ESP32-LAYER-2`;
- matching the Layer 2 128-bit Service UUID
  `7a110000-6b0d-4d5a-8f4b-2c9e00000001`;
- target RSSI and receive-count logging;
- a rate-limited target report plus a repeating scanner heartbeat;
- one-stop device profiling, build, flash, runtime verification, and
  timestamped logging through `bootload.sh`.

Layer 3 excludes:

- Advertising from Board B;
- GATT Server or GATT Client behavior;
- connecting to Board A;
- RX/TX Characteristic discovery, Read, Write, or Notify;
- Bluetooth Mesh, provisioning, keys, Models, or Mesh Relay;
- custom relay packets, TTL, deduplication, or retransmission;
- multiple-board routing logic.

## Why Active Scanning

Layer 2 transmits its Service UUID in the primary Advertising packet and its
local name in the scan response. Layer 3 therefore uses active scanning:

```text
Board B receives primary Advertising
  -> sees Layer 2 Service UUID
Board B sends scan request
  -> Board A returns scan response
  -> sees ESP32-LAYER-2 local name
```

A passive scanner could match the Service UUID but is not sufficient for this
Layer's name-and-UUID proof.

## Scan Configuration

Layer 3 uses legacy BLE 4.2 scanning compatible with Layer 2:

- active scan type;
- public own-address type;
- allow all advertising devices;
- scan interval 100 ms;
- scan window 80 ms;
- duplicate filtering disabled so repeated physical receptions can be
  counted;
- continuous scan duration.

The GAP callback remains short. It validates lengths, extracts the local name
and complete or incomplete 128-bit Service UUID list, updates bounded target
state, and returns. It does not allocate unbounded memory or print every
unrelated nearby device.

## Target Matching

The target is confirmed only when the same scan result contains both:

- local name equal to `ESP32-LAYER-2`;
- the exact Layer 2 Service UUID.

Name-only or UUID-only observations may be logged as partial matches but do
not set the final target-confirmed flag. All comparisons use explicit lengths;
Advertising bytes are never treated as null-terminated strings.

## Observable Serial Contract

Stable initialization markers:

```text
[LAYER-3] BOOT_SUCCESS
[LAYER-3] NVS_READY
[LAYER-3] BLE_CONTROLLER_ENABLED
[LAYER-3] BLUEDROID_ENABLED
[LAYER-3] SCAN_PARAMS_READY
[LAYER-3] SCANNING_STARTED mode=active
```

First complete target reception:

```text
[LAYER-3] TARGET_FOUND name=ESP32-LAYER-2 rssi=-45
[LAYER-3] SERVICE_MATCH uuid=7A110000-6B0D-4D5A-8F4B-2C9E00000001
[LAYER-3] TARGET_RX count=1 rssi=-45
[LAYER-3] SCAN_TARGET_CONFIRMED
```

Repeating heartbeat:

```text
[LAYER-3] SCANNER_ACTIVE target_count=1 last_rssi=-45
```

Target reports after the first reception are rate-limited to avoid serial-log
flooding while the receive count continues to increase.

## One-stop Workflow

`layers/layer-3/bootload.sh` follows the established Layer workflow:

1. verify required commands and ESP-IDF installation;
2. discover one USB serial device or accept an explicit `--port`;
3. record the USB profile;
4. verify ESP32-S3 identity and 16 MB flash;
5. configure the `esp32s3` target when necessary;
6. build and verify bootloader, partition table, and application artifacts;
7. flash Board B and rely on esptool hash verification;
8. capture the reboot log with a finite timeout;
9. require `[LAYER-3] TARGET_RX` from Board A before passing;
10. save
    `esp32s3-layer-3-active-scanner-YYYYMMDDTHHMMSS-KST.log`.

An automatic `RESULT=PASS` proves build, flash, reboot, scanner start, and one
actual Board A to Board B over-the-air target reception. If Board A is off,
connected to a phone, too far away, or not running Layer 2, the workflow must
time out with `RESULT=FAIL` at `target_reception_verification`.

## Failure Handling

- Required initialization and GAP registration calls fail fast through
  checked ESP-IDF return values.
- Scan-parameter or scan-start callback failures emit stable failure markers.
- Advertising and scan-response lengths are checked before parsing.
- Target state is updated only after full name and UUID comparison.
- The script refuses ambiguous automatic selection when multiple serial ports
  are present.
- The script never runs `erase-flash` or erases NVS automatically.
- Every script failure records the failing stage, nonzero exit code, and log
  path.

## Verification

Static and build verification:

- `bash -n layers/layer-3/bootload.sh`;
- `shellcheck layers/layer-3/bootload.sh` when available;
- ESP-IDF clean build for `esp32s3`;
- expected binary artifact existence;
- source audit confirming Layer 0-2 remain unchanged.

Hardware verification:

- confirm Board A is visible as `ESP32-LAYER-2` and is not connected;
- profile Board B through its live serial device;
- run Layer 3 `bootload.sh` against Board B;
- require the target name, Service UUID, RSSI, and `TARGET_RX` marker;
- turn Board A off only after the successful run if a negative radio test is
  desired; that negative test is optional and must not be confused with the
  normal success workflow.

Build success, flash success, scanner initialization, generic radio scanning,
and exact Board A reception remain distinct evidence stages in the report.
