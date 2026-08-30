# NOSTOS 장치별 배선도

각 PNG는 독립적으로 확인할 수 있는 1600×1080 연결도이며, SVG 원본도 함께 제공합니다.
배선 기준은 저장소의 [`PINS.md`](../../PINS.md)입니다.

| 장치 | PNG | SVG |
| --- | --- | --- |
| 전체 미리보기 | [00-all-wiring-overview.png](00-all-wiring-overview.png) | — |
| ESP32-S3 UART | [01-esp32-uart.png](01-esp32-uart.png) | [01-esp32-uart.svg](01-esp32-uart.svg) |
| SSD1306 OLED | [02-ssd1306-i2c.png](02-ssd1306-i2c.png) | [02-ssd1306-i2c.svg](02-ssd1306-i2c.svg) |
| VS1003B | [03-vs1003b-spi.png](03-vs1003b-spi.png) | [03-vs1003b-spi.svg](03-vs1003b-spi.svg) |
| Buttons | [04-buttons.png](04-buttons.png) | [04-buttons.svg](04-buttons.svg) |
| RGB LED | [05-rgb-led.png](05-rgb-led.png) | [05-rgb-led.svg](05-rgb-led.svg) |
| Buzzer | [06-buzzer.png](06-buzzer.png) | [06-buzzer.svg](06-buzzer.svg) |
| MPU6050 | [07-mpu6050-i2c.png](07-mpu6050-i2c.png) | [07-mpu6050-i2c.svg](07-mpu6050-i2c.svg) |
| DHT11 | [08-dht11.png](08-dht11.png) | [08-dht11.svg](08-dht11.svg) |

## 배선 주의사항

- 모든 디지털 신호는 3.3 V 로직이며 모든 보드와 모듈은 GND를 공유합니다.
- VS1003B는 Arduino `D4/D5/D6/D7/D14/D15` 별칭으로 연결하지 않습니다. 그림의 실제 MCU 핀과
  `CN10` 번호를 사용합니다.
- Button 입력은 내부 Pull-up 기준이며 버튼의 반대쪽 단자는 GND입니다.
- RGB LED 그림은 공통 캐소드 제품 기준입니다. 공통 애노드 제품은 COM 연결과 출력 논리가 다릅니다.
- Buzzer와 VS1003B의 전원은 사용하는 브레이크아웃 보드의 정격을 우선 확인합니다.
- DHT11 센서 단품은 DATA–3V3 사이 Pull-up 저항이 필요할 수 있습니다.

## 다시 생성하기

```bash
python3 docs/wiring-diagrams/generate_diagrams.py
for diagram_svg in docs/wiring-diagrams/*.svg; do
  sips -s format png "$diagram_svg" --out "${diagram_svg%.svg}.png"
done
```
