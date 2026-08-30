# Development and Validation Workflow

Complete each phase and meet its exit criteria before moving to the next phase.

## Phase 1: Local STM32 Validation

**Objective:** Confirm that the complete firmware works correctly on one STM32 board.

1. Implement and test all required local functionality.
2. Verify each peripheral, including the buttons, RGB LED, buzzer, and display.
3. Produce the final firmware build for distribution.

**Exit criteria:** All required features pass on one STM32 without Bluetooth.

## Phase 2: STM32 Firmware Distribution

**Objective:** Reproduce the validated behavior on all three STM32 boards.

1. Deploy the complete firmware to the other two STM32 boards.
2. Repeat the local functionality and peripheral tests on each board.
3. Confirm that all three boards work correctly without Bluetooth.

**Exit criteria:** The same firmware and local features pass on all three STM32 boards.

## Phase 3: Bluetooth Mesh Validation

**Objective:** Validate the three-ESP32 Bluetooth Mesh system incrementally, from basic functions to complex scenarios.

1. Create layered mock test suites:
   - Layer 1 covers basic functions.
   - Each higher layer adds more integration and complexity.
2. Run the Layer 1 tests with all three ESP32 boards and resolve failures until the layer passes.
3. Create and run the Layer 2 test suite, then resolve failures until it passes.
4. Continue iteratively through Layer N, covering complex scenarios such as message-queue conflicts and edge cases.

**Exit criteria:** Every defined test layer passes with all three ESP32 boards.
