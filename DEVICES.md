# NOSTOS Devices

| Device | Role / current state |
| --- | --- |
| STM32 NUCLEO-F411RE | Main MCU |
| ESP32-S3-N16R8 | UART ↔ Bluetooth Mesh bridge |
| SSD1306 | OLED display, currently fitted |
| VS1003B | Audio decoder, currently fitted |
| Buttons | Speed Up, Speed Down, Stop Request; Button 4 message/Buzzer Off not implemented |
| RGB LED | Visual status output |
| Buzzer | Confirmed fall alert output |
| MPU6050 | Optional local fall-detection sensor; raw motion data is not shared |
| DHT11 | Optional temperature/humidity sensor, currently not fitted |

배선과 활성화 상태는 [PINS.md](PINS.md)를 따릅니다.
