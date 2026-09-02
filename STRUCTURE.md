# NOSTOS 구조

```text
nostos/
├── AGENTS.md
├── README.md
├── STRUCTURE.md
├── REQUIREMENT.md
├── CONTEXT.md
├── DEVICES.md
├── PINS.md
├── RIDER-1.md
├── RIDER-2.md
├── RIDER-3.md
├── apps/
│   ├── mesh-console/
│   └── nostos-hardware-monitor/
├── docs/
│   ├── bluetooth-setting/
│   ├── adr/
│   ├── media/
│   ├── study/
│   └── schematics/
│       └── nostos/
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
│   │   ├── CMakeLists.txt
│   │   ├── nostos_protocol.[ch]
│   │   ├── nostos_uart.[ch]
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
