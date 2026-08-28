> 이관 원문: `docs/superpowers/specs/2026-08-27-layer-2-gatt-server-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 2: Basic BLE GATT Server Design

Date: 2026-08-27
Target: ESP32-S3 N16R8
SDK: ESP-IDF v5.5.5, Bluedroid host

## Goal

Layer 2 extends the proven Layer 1 boot-and-advertise workflow with the
smallest useful connection-oriented BLE exchange:

```text
phone central
  -> connect to ESP32-LAYER-2
  -> write HELLO to RX
  <- receive ACK:HELLO from TX notification
  <- read the same latest TX value
```

Layer 2 is an independent ESP-IDF project under `layers/layer-2`. It does not
modify Layer 0 or Layer 1.

## Scope

Layer 2 includes:

- connectable BLE Advertising under the name `ESP32-LAYER-2`;
- one custom GATT Service;
- one RX Characteristic supporting Write;
- one TX Characteristic supporting Read and Notify;
- a CCCD for enabling or disabling TX notifications;
- a bounded application payload of 20 bytes;
- serial logs for boot, GATT setup, connection, Write, Read, notification,
  disconnection, and Advertising restart;
- one-stop device profiling, build, flash, boot-log validation, and log capture
  through `bootload.sh`.

Layer 2 explicitly excludes:

- ESP32 scanning or Central/GATT Client behavior;
- multiple simultaneous connections;
- pairing, bonding, or application authentication;
- standard Bluetooth Mesh, provisioning, keys, or Models;
- multi-device relay and TTL/deduplication logic;
- production framing, CRC, persistence, and OTA.

## BLE Interface

The project uses fixed 128-bit custom UUIDs so that nRF Connect can identify
the attributes consistently:

| Attribute | UUID suffix | Properties | Direction |
| --- | --- | --- | --- |
| Layer 2 Service | `...0001` | Primary Service | container |
| RX Characteristic | `...0002` | Write | phone -> ESP32 |
| TX Characteristic | `...0003` | Read, Notify | ESP32 -> phone |

All three UUIDs share the base `7a110000-6b0d-4d5a-8f4b-2c9e00000000`; the
last 32-bit field is `00000001`, `00000002`, or `00000003` respectively.

RX accepts 1 through 20 bytes. The first learning test uses UTF-8/ASCII text:

```text
RX Write:  HELLO
TX value:  ACK:HELLO
TX Notify: ACK:HELLO
```

The response is always bounded to 20 bytes. Empty and oversized writes are
rejected and logged. A valid RX Write updates TX even when notification is not
enabled. A notification is sent only while connected and after the phone has
enabled the TX CCCD; otherwise the updated value remains available through
Read.

## Firmware Architecture

`app_main()` performs the same staged initialization as Layer 1:

1. initialize NVS without erasing existing storage;
2. release unused Classic Bluetooth memory;
3. initialize and enable the BLE Controller;
4. initialize and enable Bluedroid;
5. register GAP and GATT Server callbacks;
6. register the GATT application.

The GAP callback owns Advertising configuration and start/stop results. The
GATT Server callback owns Service creation, Characteristic/CCCD creation,
connection state, attribute access, and notifications.

Advertising starts only after both conditions are true:

- Advertising data configuration completed successfully;
- the Service, RX, TX, and TX CCCD handles are ready.

On disconnect, the firmware clears connection and notification state and
starts connectable Advertising again.

## Observable Serial Contract

The automatic workflow uses stable markers rather than depending on incidental
ESP-IDF logs:

```text
[LAYER-2] BOOT_SUCCESS
[LAYER-2] NVS_READY
[LAYER-2] BLE_CONTROLLER_ENABLED
[LAYER-2] BLUEDROID_ENABLED
[LAYER-2] GATT_SERVICE_READY
[LAYER-2] ADVERTISING_STARTED name=ESP32-LAYER-2 type=connectable
[LAYER-2] GATT_SERVER_READY
```

Phone interaction adds:

```text
[LAYER-2] CONNECTED
[LAYER-2] NOTIFY_ENABLED
[LAYER-2] RX_WRITE len=5 value=HELLO
[LAYER-2] TX_NOTIFY len=9 value=ACK:HELLO
[LAYER-2] TX_READ len=9 value=ACK:HELLO
[LAYER-2] DISCONNECTED
[LAYER-2] ADVERTISING_RESTARTED
```

The log prints payload text only as a bounded, sanitized representation; raw
input is never treated as a C string without an explicit length and terminator.

## One-stop Workflow

`layers/layer-2/bootload.sh` follows the established Layer 1 workflow:

1. verify required commands and ESP-IDF installation;
2. discover the USB serial device or accept `--port`;
3. print the USB profile;
4. verify ESP32-S3 identity and 16 MB flash;
5. configure the `esp32s3` target when necessary;
6. build and verify bootloader, partition table, and application artifacts;
7. flash all images and rely on esptool hash verification;
8. capture the reboot log with a finite timeout;
9. require the `GATT_SERVER_READY` marker;
10. save a timestamped log named
    `esp32s3-layer-2-gatt-server-YYYYMMDDTHHMMSS-KST.log`.

An automatic `RESULT=PASS` proves source/build/flash/reboot/GATT setup and
connectable Advertising initialization. It does not claim phone connection,
Write, Read, or Notify success. The summary therefore prints
`PHONE_GATT_TEST=NOT_VERIFIED` until the manual phone test is performed.

## Failure Handling

- Required initialization and registration calls fail fast through checked
  ESP-IDF return values.
- Callback failures emit a stable `[LAYER-2] ..._FAILED` marker and do not set
  the ready flag.
- GATT handle use is gated on successful creation.
- RX length validation occurs before copying.
- Notification requires a live connection and enabled CCCD.
- Disconnect always clears stale connection identifiers and subscription
  state before Advertising restarts.
- The script exits nonzero, records `RESULT=FAIL`, and records the failing
  stage when any automatic stage fails.

## Verification

Static and build verification:

- `bash -n layers/layer-2/bootload.sh`;
- `shellcheck layers/layer-2/bootload.sh` when ShellCheck is installed;
- ESP-IDF clean build for `esp32s3`;
- expected binary artifact existence.

Hardware verification when the ESP32-S3 serial device is connected:

- run `./bootload.sh` and require the stable ready markers;
- scan for and connect to `ESP32-LAYER-2` using nRF Connect;
- enable notifications on TX;
- Write UTF-8 `HELLO` to RX;
- observe `ACK:HELLO` as a TX notification;
- Read TX and observe the same value;
- disconnect and confirm that the device advertises again.

Build success, flash success, firmware boot, Advertising, phone connection,
Write, Read, and Notify remain separate evidence stages in the final report.
