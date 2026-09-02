# NOSTOS Devices

| Device | Role / current state |
| --- | --- |
| Rider Node 1 | STM32 main MCU; ESP32-76 pair; no local MPU6050/DHT11 |
| Rider Node 2 | STM32 main MCU; ESP32-D6 pair; DHT11 temperature/humidity |
| Rider Node 3 | STM32 main MCU; ESP32-B6 pair; MPU6050 fall detection |
| ESP32-S3-N16R8 | UART ↔ Bluetooth Mesh bridge |
| SSD1306 | OLED display, currently fitted |
| VS1003B | Audio decoder, currently fitted |
| Buttons | Speed Up, Speed Down, Stop Request; Button 4 resets local outputs |
| RGB LED | Visual status output |
| Buzzer | Confirmed fall alert output |
| MPU6050 | Rider Node 3; raw motion stays local and a fall transition creates `STOP_REQUEST(reason=FALL)` |
| DHT11 | Rider Node 2; temperature/humidity are shared |

배선과 활성화 상태는 [PINS.md](PINS.md)를 따릅니다.
