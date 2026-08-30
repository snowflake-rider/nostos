# NOSTOS Devices

| Device | Role / current state |
| --- | --- |
| STM32-1 / FRONT | Main MCU; ESP32-76 pair; no local MPU6050/DHT11 |
| STM32-2 / REAR | Main MCU; ESP32-D6 pair; MPU6050 fall detection |
| STM32-3 / CENTER | Main MCU; ESP32-B6 pair; DHT11 temperature/humidity |
| ESP32-S3-N16R8 | UART ↔ Bluetooth Mesh bridge |
| SSD1306 | OLED display, currently fitted |
| VS1003B | Audio decoder, currently fitted |
| Buttons | Speed Up, Speed Down, Stop Request; Button 4 resets local outputs |
| RGB LED | Visual status output |
| Buzzer | Confirmed fall alert output |
| MPU6050 | Fitted to STM32-2; raw motion stays local and the fall state is shared |
| DHT11 | Fitted to STM32-3; temperature/humidity are shared |

배선과 활성화 상태는 [PINS.md](PINS.md)를 따릅니다.
