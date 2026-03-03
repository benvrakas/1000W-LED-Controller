# ProjectorMaster Firmware (v2.0)

Firmware for the 1000W LED Projector Controller, designed for the **Adafruit Feather M4 Express (SAMD51)**.

## Overview
This system manages the power and cooling of a high-power COB LED array. It ensures safe operation through rigorous state management, thermal monitoring, and active fault protection.

### Key Features
*   **Power Control**: Digital control of **Mean Well UHP-1500-48** PSU via Native CAN Bus (PMBus protocol).
*   **Active Cooling**: PID-free temperature control using configurable Fan Curves for Radiator Fans, Pump, and Aux Fans.
*   **User Interface**: OLED status display (128x32) and Rotary Encoder input.
*   **Safety**:
    *   Latched `ERROR_KILL` state for critical faults (Over-temp, Pump failure, CAN timeout).
    *   Hardware enable lines dropped immediately on fault.
    *   **QSPI Error Logging**: Crashes and faults are saved to onboard flash (`error_log.csv`) for post-mortem analysis.

## Hardware
*   **MCU**: Adafruit Feather M4 Express (ATSAMD51).
*   **CAN**: Integrated SAMD51 CAN Controller + ISO1050 Transceiver (SDA/SCL pins).
*   **Storage**: 2MB QSPI Flash (GD25Q16).

## Directory Structure
*   `src/` - Implementation files.
    *   `core/` - Hardware instantiation (`Hardware.cpp`) and dependency injection (`AppContext`).
    *   `services/` - Business logic (`CoolingService`, `PsuService`).
    *   `drivers/` - Hardware abstraction (`NativeCanBackend`, `OLED`).
    *   `state/` - Finite State Machine (`Init`, `Run`, `Error`).
*   `include/config/` - System configuration.
    *   `PinMap.h` - Pin definitions.
    *   `ThermalConfig.h` - Fan curves and temperature limits.
    *   `PowerConfig.h` - PSU and CAN settings.
*   `docs/` - Documentation.
    *   `ARCHITECTURE.md` - Detailed architectural design.

## Building & Flashing
This project uses **PlatformIO**.

1.  **Build**: `pio run`
2.  **Upload**: `pio run -t upload`
3.  **Monitor**: `pio device monitor`

## License
Proprietary / Internal Use.
