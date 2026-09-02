# 1000W LED Controller Pinout

This document details the pin mappings for the Adafruit Feather M4 CAN Express board used in the 1000W LED Controller.

## Pin Assignments

| Function | Arduino / CircuitPython Pin | MCU GPIO | Hardware Connected To | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Cooling & Fans** |
| Rad Fan PWM | D5 | PA16 | Radiator Fan PWM Control | |
| Pump PWM | D6 | PA18 | Water Pump PWM Control | |
| PSU Fan PWM | D9 | PA19 | Power Supply Fan PWM Control | |
| Aux Fan PWM | D4 | PA14 | Auxiliary Lens Cooling Fan PWM | |
| Rad Fan Tach | D10 | PA20 | Radiator Fan Tachometer | EIC 4 |
| Pump Tach | D11 | PA21 | Water Pump Tachometer | EIC 5 |
| PSU Fan Tach | D12 | PA22 | Power Supply Fan Tachometer | EIC 6 |
| Aux Fan Tach | D13 | PA23 | Auxiliary Lens Cooling Fan Tachometer | EIC 7 |
| **Sensors & Input** |
| LED Temp | A0 | PA02 | LED Thermistor | 10K+10K voltage divider to ground |
| Water Temp | A1 | PA05 | Water Thermistor | 10K+10K voltage divider to ground |
| Encoder A | RX | PB17 | Rotary Encoder Channel A | EIC 1 |
| Encoder B | TX | PB16 | Rotary Encoder Channel B | EIC 0 |
| Switch Signal | A3 | PB09 | Power Button Signal | EIC 9 |
| **Outputs & Control** |
| Switch LED | A2 | PB08 | Power Button Status LED | |
| PSU Remote On/Off | A4 | PA04 | Remote1 & Therm_sw1 | Remote1 connects to UHP 1500 Remote On/Off & +12Vaux. In series with Therm_sw1 (optional hardware thermal cutoff switch). Therm_sw1 can be bridged and ignored. |
| PSU PC (current prog) | A5 | PA06 | UHP-1500-48 CN71 pin 2 | PWM out → external RC low-pass → PC pin. Only used when `PsuControlConfig::PSU_CONTROL_VIA_CAN == false` (analog/blind mode). See `AnalogPsuBackend`/`AnalogPsuConfig` for the transfer curve. |
| **Display (I2C)** |
| OLED SDA | SDA | PA12 | Feather Wing OLED SDA | |
| OLED SCL | SCL | PA13 | Feather Wing OLED SCL | |
| **Unused (Free)** |
| Free | MOSI | PB23 | None | Available (SPI bus has no devices) |
| Free | MISO | PB22 | None | Reserved for future DC-OK input from CN71 pin 6 |
| Free | SCK | PA17 | None | Available |

## Internal Board Pins (Feather M4 CAN)
- **CAN TX/RX**: Handled internally by the ATSAME51.
- **CAN Standby**: `PIN_CAN_STANDBY` (Internal)
- **CAN Boost Enable**: `PIN_CAN_BOOSTEN` (Internal)
