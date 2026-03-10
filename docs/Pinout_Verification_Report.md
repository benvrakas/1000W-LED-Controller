# Pinout Verification Report

This report verifies the hardware pin connections across the software (`include/config/PinMap.h`), documentation (`docs/Pinout.md`), and the KiCad PCB design (`Kicad/1000W Controller.kicad_pcb`).

## Overview
All pin mappings are **CONFIRMED** to match exactly across the software, documentation, and PCB layout. No functional discrepancies were found. 

## Detailed Pin Verification

| Function | Software Pin (`PinMap.h`) | Documented Pin (`Pinout.md`) | PCB Connection (`.kicad_pcb`) | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Cooling & Fans** |
| Rad Fan PWM | `PIN_RAD_FANS_PWM = 5` | D5 | `Feather_M4_Express1` Pad 19 (D5) -> `U1` Pad 3 | ✅ Match | Passes through isolator U1 |
| Pump PWM | `PIN_PUMP_PWM = 6` | D6 | `Feather_M4_Express1` Pad 20 (D6) -> `U5` Pad 3 | ✅ Match | Passes through isolator U5 |
| PSU Fan PWM | `PIN_PSU_FAN_PWM = 9` | D9 | `Feather_M4_Express1` Pad 21 (D9) -> `U4` Pad 3 | ✅ Match | Passes through isolator U4 |
| Aux Fan PWM | `PIN_AUX_FAN_PWM = 4` | D4 | `Feather_M4_Express1` Pad 16 (D4) -> `U6` Pad 3 | ✅ Match | Passes through isolator U6 |
| Rad Fan Tach | `PIN_RAD_FAN_TACH = 10` | D10 | `Feather_M4_Express1` Pad 22 (D10) -> `U1` Pad 2 | ✅ Match | Received via isolator U1 |
| Pump Tach | `PIN_PUMP_TACH = 11` | D11 | `Feather_M4_Express1` Pad 23 (D11) -> `U5` Pad 2 | ✅ Match | Received via isolator U5 |
| PSU Fan Tach | `PIN_PSU_FAN_TACH = 12` | D12 | `Feather_M4_Express1` Pad 24 (D12) -> `U4` Pad 2 | ✅ Match | Received via isolator U4 |
| Aux Fan Tach | `PIN_AUX_FAN_TACH = 13` | D13 | `Feather_M4_Express1` Pad 25 (D13) -> `U6` Pad 2 | ✅ Match | Received via isolator U6 |
| **Sensors & Input** |
| LED Temp | `PIN_THERM_LED = A0` | A0 | `Feather_M4_Express1` Pad 5 (A0) -> `LED_TEMP1` Pad 2 | ✅ Match | |
| Water Temp | `PIN_THERM_WATER = A1` | A1 | `Feather_M4_Express1` Pad 6 (A1) -> `WATER_TEMP1` Pad 2 | ✅ Match | |
| Encoder A | `PIN_ENCODER_A = 0` | RX | `Feather_M4_Express1` Pad 14 (RX) -> `Encoder1` Pad 3 | ✅ Match | |
| Encoder B | `PIN_ENCODER_B = 1` | TX | `Feather_M4_Express1` Pad 15 (TX) -> `Encoder1` Pad 2 | ✅ Match | |
| Switch Signal | `PIN_SW_BTN = A3` | A3 | `Feather_M4_Express1` Pad 8 (A3) -> `Switch1` Pad 4 | ✅ Match | Filter circuit components (R12, C17) also present |
| **Outputs & Control** |
| Switch LED | `PIN_SW_LED = A2` | A2 | `Feather_M4_Express1` Pad 7 (A2) -> `R3` -> `Switch1` Pad 1 | ✅ Match | Includes current limiting resistor R3 |
| PSU Remote On/Off | `PIN_PSU_REMOTE = A4` | A4 | `Feather_M4_Express1` Pad 9 (A4) -> `Therm_sw1` Pad 2 | ✅ Match | **Remote1** is the connector for PSU Remote On/Off and +12Vaux. This is in series with **Therm_sw1**, an optional hardware thermal cutoff switch (which can be bridged and ignored). |
| **Display (I2C)** |
| OLED SDA | `PIN_OLED_SDA = SDA` | SDA | `Feather_M4_Express1` I2C SDA Pad -> `Display1` Pad | ✅ Match | |
| OLED SCL | `PIN_OLED_SCL = SCL` | SCL | `Feather_M4_Express1` I2C SCL Pad -> `Display1` Pad | ✅ Match | |

## Minor Observations
* The schematic component connecting to `A4` (PSU Remote On/Off) is labeled as `Therm_sw1`. As noted above, this is in series with the MCU connection to the MOSFET that controls the `Remote1` connector which handles the actual remote on/off and +12Vaux for the PSU.
* The pinout and schematic reflect correct placement of signal isolation barriers (`ADuM1201`) for all high-power component controls (PWM and Tach lines).