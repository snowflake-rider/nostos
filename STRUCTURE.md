# NOSTOS 구조

```text
nostos/
├── AGENTS.md
├── README.md
├── STRUCTURE.md
├── REQUIREMENT.md
├── DEVICES.md
├── PINS.md
├── RIDER-HEAD.md
├── RIDER-MID.md
├── RIDER-TAIL.md
├── apps/
│   ├── mesh-console/
│   └── nostos-hardware-monitor/
├── docs/
│   ├── bluetooth-setting/
│   └── media/
├── firmware/
│   ├── README.md
│   ├── VERSION
│   ├── build.sh
│   ├── test-host.sh
│   ├── tools/
│   │   ├── fw
│   │   ├── release.py
│   │   └── test_release.py
│   ├── profiles/
│   │   └── release.json
│   ├── inventory/
│   │   └── boards.example.json
│   ├── protocol/
│   │   ├── README.md
│   │   ├── V2.md
│   │   ├── CMakeLists.txt
│   │   ├── *.c
│   │   ├── *.h
│   │   └── tests/
│   ├── stm32/
│   │   ├── CMakeLists.txt
│   │   ├── CMakePresets.json
│   │   ├── Core/
│   │   ├── Drivers/
│   │   ├── MyApp/
│   │   ├── cmake/
│   │   └── tools/
│   └── esp32/
│       ├── CMakeLists.txt
│       ├── main/
│       ├── host-tests/
│       ├── sdkconfig
│       └── sdkconfig.defaults*
└── releases/
    ├── README.md
    ├── index.json
    └── baselines/
```
