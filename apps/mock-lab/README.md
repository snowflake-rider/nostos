# NOSTOS Mock Lab

A browser-based simulator for three paired STM32 and ESP32 device lanes.

## Run locally

```bash
npm install
npm run dev
```

Open the local URL printed by Vite.

## Simulated controls

- `B1` sends `MSG_SPEED_UP_REQUEST` and shows green RGB output.
- `B2` sends `MSG_SPEED_DOWN_REQUEST` and shows yellow RGB output.
- `B3` sends `MSG_STOP_REQUEST`, shows red RGB output, and is the only button that activates the simulated buzzer.
- STM32-02 includes a DHT11 temperature/humidity control. Its SSD1306 reproduces the current `NOSTOS SENSOR`, `TEMP`, `HUM`, and `DHT OK` firmware strings.
- STM32-03 includes an MPU6050 motion sampler. The current display firmware does not render MPU6050 data, so its SSD1306 keeps the `NOSTOS NODE` / `DHT NOT FITTED` output.
- Use **Run scenario** to exercise all three board pairs in sequence.
- Use **Live** to pause or resume board inputs, and **Reset all** to restore the idle state.

This is a host-side UI simulator. It does not flash, provision, or communicate with physical boards.
