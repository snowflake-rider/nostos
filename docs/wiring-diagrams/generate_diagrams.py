#!/usr/bin/env python3
"""Generate deterministic NOSTOS wiring diagrams as SVG files."""

from __future__ import annotations

from html import escape
from pathlib import Path


OUT_DIR = Path(__file__).resolve().parent
WIDTH = 1600
HEIGHT = 1080


DIAGRAMS = [
    {
        "slug": "01-esp32-uart",
        "title": "ESP32-S3 ↔ STM32 UART 연결",
        "subtitle": "UART1 · 115200 baud · 8N1 · TX/RX 교차 연결",
        "device": "ESP32-S3",
        "connections": [
            ("D8 / PA9\nUSART1_TX", "GPIO18\nUART1_RX", "STM32 → ESP32", "#E76F51"),
            ("D2 / PA10\nUSART1_RX", "GPIO17\nUART1_TX", "ESP32 → STM32", "#2A9D8F"),
            ("GND", "GND", "공통 접지", "#374151"),
        ],
        "notes": [
            "ESP32-S3는 USB 또는 보드 규격 전원으로 공급하고, 두 보드의 GND는 반드시 연결",
            "UART 신호는 3.3 V 로직 · Hardware Flow Control 없음",
        ],
    },
    {
        "slug": "02-ssd1306-i2c",
        "title": "SSD1306 OLED ↔ STM32 I²C 연결",
        "subtitle": "I2C1 · 기본 주소 0x3C",
        "device": "SSD1306 OLED",
        "connections": [
            ("D15 / PB8\nI2C1_SCL", "SCL", "Clock", "#3A86FF"),
            ("D14 / PB9\nI2C1_SDA", "SDA", "Data", "#8B5CF6"),
            ("3V3", "VCC", "3.3 V", "#E63946"),
            ("GND", "GND", "공통 접지", "#374151"),
        ],
        "notes": [
            "PB8/PB9는 MPU6050과 같은 I2C1 버스를 공유 가능",
            "모듈 전원 허용 범위는 제품 표기를 우선 확인",
        ],
    },
    {
        "slug": "03-vs1003b-spi",
        "title": "VS1003B ↔ STM32 SPI2 연결",
        "subtitle": "SPI2 + 제어 신호 · D 별칭 대신 실제 MCU 핀/CN10 번호 사용",
        "device": "VS1003B",
        "connections": [
            ("CN10-24 / PB1\nGPIO Output", "XRST", "Reset", "#EF476F"),
            ("CN10-28 / PB14\nSPI2_MISO", "MISO", "VS1003B → STM32", "#2A9D8F"),
            ("CN10-26 / PB15\nSPI2_MOSI", "MOSI", "STM32 → VS1003B", "#E76F51"),
            ("CN10-30 / PB13\nSPI2_SCK", "SCLK", "Clock", "#3A86FF"),
            ("CN10-6 / PC5\nGPIO Input", "DREQ", "VS1003B → STM32", "#06A77D"),
            ("CN10-4 / PC6\nGPIO Output", "XCS", "SCI CS · Active Low", "#F59E0B"),
            ("CN10-2 / PC8\nGPIO Output", "XDCS", "SDI CS · Active Low", "#A855F7"),
            ("GND", "GND", "공통 접지", "#374151"),
        ],
        "notes": [
            "주의: PB13/PB14/PB15/PC5/PC6/PC8은 NUCLEO D4/D5/D6/D7/D14/D15가 아님",
            "VS1003B 모듈의 VCC는 해당 브레이크아웃 보드 사양에 맞춰 별도 연결",
        ],
        "warning": True,
    },
    {
        "slug": "04-buttons",
        "title": "버튼 4개 ↔ STM32 연결",
        "subtitle": "내부 Pull-up 사용 · 버튼을 누르면 GPIO가 Low",
        "device": "Push Buttons",
        "connections": [
            ("D4 / PB5\nGPIO Input", "Button 1\n반대쪽 → GND", "Speed Up", "#E76F51"),
            ("D6 / PB10\nGPIO Input", "Button 2\n반대쪽 → GND", "Speed Down", "#3A86FF"),
            ("D7 / PA8\nGPIO Input", "Button 3\n반대쪽 → GND", "Stop Request", "#EF476F"),
            ("D9 / PC7\nGPIO Input", "Button 4\n반대쪽 → GND", "Buzzer Off", "#8B5CF6"),
        ],
        "notes": [
            "각 버튼: 한쪽 단자 = GPIO, 다른 쪽 단자 = GND",
            "현재 PINS.md 기준 Button 4 메시지 동작은 미구현(MSG_NONE)",
        ],
    },
    {
        "slug": "05-rgb-led",
        "title": "RGB LED ↔ STM32 연결",
        "subtitle": "공통 캐소드 RGB LED 예시 · 각 색상에 전류 제한 저항 필요",
        "device": "RGB LED",
        "connections": [
            ("A2 / PA4\nGPIO Output", "220–330 Ω → R", "Red", "#E63946"),
            ("A3 / PB0\nGPIO Output", "220–330 Ω → G", "Green", "#22C55E"),
            ("A4 / PC1\nGPIO Output", "220–330 Ω → B", "Blue", "#3A86FF"),
            ("GND", "COM / Cathode", "공통 캐소드", "#374151"),
        ],
        "notes": [
            "공통 애노드 제품이면 COM을 3V3에 연결하고 출력 논리가 반대로 동작",
            "RGB LED 모듈이면 내장 저항 유무와 R/G/B/COM 핀 순서를 먼저 확인",
        ],
    },
    {
        "slug": "06-buzzer",
        "title": "Buzzer ↔ STM32 연결",
        "subtitle": "D5 / PB4 출력",
        "device": "Buzzer Module",
        "connections": [
            ("D5 / PB4\nGPIO Output", "SIG / IN", "Buzzer control", "#F59E0B"),
            ("3V3", "VCC", "3.3 V 모듈", "#E63946"),
            ("GND", "GND", "공통 접지", "#374151"),
        ],
        "notes": [
            "3핀 Active Buzzer 모듈 예시 · 모듈 정격 전압을 먼저 확인",
            "고전류 또는 2핀 부저를 GPIO에 직접 연결하지 말고 트랜지스터 드라이버 사용",
        ],
        "warning": True,
    },
    {
        "slug": "07-mpu6050-i2c",
        "title": "MPU6050 ↔ STM32 I²C 연결",
        "subtitle": "I2C1 · 주소 0x68 (AD0=Low) / 0x69 (AD0=High)",
        "device": "MPU6050",
        "connections": [
            ("D15 / PB8\nI2C1_SCL", "SCL", "Clock", "#3A86FF"),
            ("D14 / PB9\nI2C1_SDA", "SDA", "Data", "#8B5CF6"),
            ("3V3", "VCC", "3.3 V", "#E63946"),
            ("GND", "GND", "공통 접지", "#374151"),
        ],
        "notes": [
            "PB8/PB9는 SSD1306과 같은 I2C1 버스를 공유 가능",
            "모듈 전원 허용 범위와 AD0 상태를 제품 표기에서 확인",
        ],
    },
    {
        "slug": "08-dht11",
        "title": "DHT11 ↔ STM32 연결",
        "subtitle": "단일 데이터 신호 · 3.3 V",
        "device": "DHT11",
        "connections": [
            ("A1 / PA1\nGPIO Data", "DATA", "Bidirectional data", "#2A9D8F"),
            ("3V3", "VCC", "3.3 V", "#E63946"),
            ("GND", "GND", "공통 접지", "#374151"),
        ],
        "notes": [
            "센서 단품은 DATA–3V3 사이에 4.7–10 kΩ Pull-up 저항 권장",
            "3핀 모듈은 Pull-up 저항이 내장되어 있는지 확인",
        ],
    },
]


def multiline_text(x: int, y: float, value: str, *, anchor: str, size: int, weight: int = 600) -> str:
    lines = value.split("\n")
    line_height = size * 1.18
    start_y = y - (len(lines) - 1) * line_height / 2
    text_nodes = []
    for idx, line in enumerate(lines):
        line_y = start_y + idx * line_height
        text_nodes.append(
            f'<text x="{x}" y="{line_y:.1f}" text-anchor="{anchor}" '
            f'dominant-baseline="middle" font-size="{size}" font-weight="{weight}" '
            f'fill="#102A43">{escape(line)}</text>'
        )
    return "".join(text_nodes)


def render(diagram: dict) -> str:
    connections = diagram["connections"]
    count = len(connections)
    panel_y = 220
    panel_h = 660
    if count >= 7:
        row_top, row_bottom, row_h, label_size = 335, 845, 60, 20
    elif count <= 3:
        row_top, row_bottom, row_h, label_size = 360, 810, 82, 23
    else:
        row_top, row_bottom, row_h, label_size = 350, 820, 78, 23
    row_gap = (row_bottom - row_top) / max(count - 1, 1)
    left_x, left_w = 100, 510
    right_x, right_w = 990, 510
    wire_x1, wire_x2 = left_x + left_w, right_x

    body = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
        "<defs>",
        "<style>text { font-family: 'Apple SD Gothic Neo', sans-serif; }</style>",
        '<filter id="shadow" x="-20%" y="-20%" width="140%" height="140%"><feDropShadow dx="0" dy="8" stdDeviation="12" flood-color="#0F172A" flood-opacity="0.14"/></filter>',
        '<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#F8FBFF"/><stop offset="1" stop-color="#EEF5FA"/></linearGradient>',
        "</defs>",
        '<rect width="1600" height="1080" fill="url(#bg)"/>',
        '<rect x="64" y="52" width="1472" height="980" rx="38" fill="#FFFFFF" filter="url(#shadow)"/>',
        f'<text x="800" y="124" text-anchor="middle" font-size="48" font-weight="800" fill="#0B2239">{escape(diagram["title"])}</text>',
        f'<text x="800" y="172" text-anchor="middle" font-size="25" font-weight="500" fill="#526579">{escape(diagram["subtitle"])}</text>',
        f'<rect x="{left_x}" y="{panel_y}" width="{left_w}" height="{panel_h}" rx="28" fill="#F4F9FC" stroke="#9FB9CB" stroke-width="3"/>',
        f'<rect x="{right_x}" y="{panel_y}" width="{right_w}" height="{panel_h}" rx="28" fill="#F4F9FC" stroke="#9FB9CB" stroke-width="3"/>',
        f'<path d="M {left_x+28} {panel_y} H {left_x+left_w-28} Q {left_x+left_w} {panel_y} {left_x+left_w} {panel_y+28} V {panel_y+78} H {left_x} V {panel_y+28} Q {left_x} {panel_y} {left_x+28} {panel_y} Z" fill="#173F5F"/>',
        f'<path d="M {right_x+28} {panel_y} H {right_x+right_w-28} Q {right_x+right_w} {panel_y} {right_x+right_w} {panel_y+28} V {panel_y+78} H {right_x} V {panel_y+28} Q {right_x} {panel_y} {right_x+28} {panel_y} Z" fill="#173F5F"/>',
        f'<text x="{left_x+left_w/2:.0f}" y="{panel_y+52}" text-anchor="middle" font-size="29" font-weight="750" fill="#FFFFFF">STM32 NUCLEO-F411RE</text>',
        f'<text x="{right_x+right_w/2:.0f}" y="{panel_y+52}" text-anchor="middle" font-size="29" font-weight="750" fill="#FFFFFF">{escape(diagram["device"])}</text>',
    ]

    for idx, (left, right, badge, color) in enumerate(connections):
        y = row_top + idx * row_gap
        body.extend(
            [
                f'<rect x="{left_x+28}" y="{y-row_h/2:.1f}" width="{left_w-56}" height="{row_h}" rx="15" fill="#FFFFFF" stroke="#D9E5ED"/>',
                f'<rect x="{right_x+28}" y="{y-row_h/2:.1f}" width="{right_w-56}" height="{row_h}" rx="15" fill="#FFFFFF" stroke="#D9E5ED"/>',
                f'<line x1="{wire_x1}" y1="{y:.1f}" x2="{wire_x2}" y2="{y:.1f}" stroke="{color}" stroke-width="8" stroke-linecap="round"/>',
                f'<circle cx="{wire_x1}" cy="{y:.1f}" r="11" fill="{color}" stroke="#FFFFFF" stroke-width="4"/>',
                f'<circle cx="{wire_x2}" cy="{y:.1f}" r="11" fill="{color}" stroke="#FFFFFF" stroke-width="4"/>',
                f'<rect x="665" y="{y-23:.1f}" width="270" height="46" rx="23" fill="#FFFFFF" stroke="{color}" stroke-width="3"/>',
                f'<text x="800" y="{y+8:.1f}" text-anchor="middle" font-size="20" font-weight="700" fill="{color}">{escape(badge)}</text>',
                multiline_text(left_x + 56, y, left, anchor="start", size=label_size, weight=700),
                multiline_text(right_x + 56, y, right, anchor="start", size=label_size, weight=700),
            ]
        )

    warning = diagram.get("warning", False)
    note_fill = "#FFF7ED" if warning else "#EFF8FF"
    note_stroke = "#FB923C" if warning else "#7CB7DC"
    note_icon = "주의" if warning else "CHECK"
    note_icon_fill = "#C2410C" if warning else "#16658A"
    body.extend(
        [
            f'<rect x="100" y="916" width="1400" height="86" rx="22" fill="{note_fill}" stroke="{note_stroke}" stroke-width="2"/>',
            f'<rect x="126" y="939" width="94" height="40" rx="20" fill="{note_icon_fill}"/>',
            f'<text x="173" y="966" text-anchor="middle" font-size="18" font-weight="800" fill="#FFFFFF">{note_icon}</text>',
            f'<text x="245" y="950" font-size="21" font-weight="650" fill="#223A4D">• {escape(diagram["notes"][0])}</text>',
            f'<text x="245" y="980" font-size="21" font-weight="650" fill="#223A4D">• {escape(diagram["notes"][1])}</text>',
            '<text x="1495" y="1022" text-anchor="end" font-size="17" font-weight="600" fill="#7A8C9A">NOSTOS · 배선 전 전원 OFF · 모든 신호 3.3 V 로직</text>',
            "</svg>",
        ]
    )
    return "\n".join(body) + "\n"


def main() -> None:
    for diagram in DIAGRAMS:
        path = OUT_DIR / f'{diagram["slug"]}.svg'
        path.write_text(render(diagram), encoding="utf-8")
        print(path.name)


if __name__ == "__main__":
    main()
