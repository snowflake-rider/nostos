# XOSS speed sensor isolated parser

This directory owns the host-tested parser for the XOSS `S-26518`. The active
ESP32 build reuses `speed_sensor.c/.h` when `CONFIG_NOSTOS_XOSS_SPEED_SENSOR=y`;
the ESP-IDF GATT producer remains under `firmware/esp32/main/xoss_ble.c`.

## Confirmed device contract

- Cycling Speed and Cadence service: `0x1816`
- CSC Measurement notification: `0x2A5B`
- CSC Feature: `0x2A5C`, captured value `01 00` (wheel data only)
- Measurement byte order: little endian
- Event time unit: `1/1024` second

The four captured notifications are preserved in `test_speed_sensor.c`. With
the temporary arm-swing circumference of `5100 mm`, they produce `22.6`,
`22.8`, and `22.8 km/h` after the first baseline sample. Their `16` detected
rotations also produce a trip distance of `81.6 m` (`16 * 5100 mm`). The
circumference is an input to `speed_sensor_update()` and is not fixed in the
parser. Replace it with the measured wheel circumference for bicycle use.

`speed_sensor_sample_t` returns the current rotation delta, instantaneous
`kmh_x10`, and accumulated `distance_mm`. Distance starts at zero on the first
baseline. A sensor reset or decreasing cumulative revolution count causes a
rebaseline without adding a large false distance; the existing trip
distance is retained. Calling `speed_sensor_reset()` starts a new boot-local
trip and clears the distance.

The active BLE consumer forwards speed and cumulative wheel distance together
as one atomic local `RIDE` sample. The parser retains `uint64_t` distance for
overflow-safe accumulation; `xoss_ble.c` forwards it only while it fits the
protocol's `uint32_t distance_mm`. Disconnect and stale notifications produce
one invalid all-zero `RIDE` sample.

## Build and test

```sh
cmake -S firmware/esp32/xoss-speed-sensor \
  -B /tmp/nostos-xoss-speed-sensor-debug \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/nostos-xoss-speed-sensor-debug --parallel
ctest --test-dir /tmp/nostos-xoss-speed-sensor-debug --output-on-failure
```

Sanitizer validation:

```sh
cmake -S firmware/esp32/xoss-speed-sensor \
  -B /tmp/nostos-xoss-speed-sensor-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON
cmake --build /tmp/nostos-xoss-speed-sensor-sanitize --parallel
ctest --test-dir /tmp/nostos-xoss-speed-sensor-sanitize --output-on-failure
```

## Runtime boundary

The parser uses no dynamic allocation, floating point, ESP-IDF, FreeRTOS,
callbacks, UART, or Mesh APIs. The active `xoss_ble.c/.h` producer copies
bounded `0x2A5B` notifications into a queue, and its task calls this parser
outside the BLE callback before handing the atomic `RIDE` to the bridge.

On the current AppleClang arm64 host, the Release `speed_sensor.c` object is
`858 bytes` total (`658` text, `72` data, `128` other). Both
`speed_sensor_state_t` and `speed_sensor_sample_t` are `16 bytes` on this host.
These are only host references; the ESP32 target sizes will differ after
integration. Runtime state is caller-owned and the parser allocates no heap
memory.

No board Flash, reset, provisioning, or Mesh configuration has been performed.
The target integration therefore remains physically unverified until the
configured wheel circumference, live notifications, UART and two-node Mesh path
are tested together.
