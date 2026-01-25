# 1000W LED Controller (Feather M4 Express)

Firmware for a high-power LED projection system utilizing a **Getian 1000W COB** and a **Mean Well UHP-1500-48** PSU.

## Hardware Features
- **Microcontroller**: Adafruit Feather M4 Express (SAMD51).
- **Cooling**: 4-Zone PWM (Rad Fans, PSU Fan, Water Pump, Aux Lens Fan).
- **Communication**: PMBus (Isolated I2C) to UHP-1500.
- **Safety**: Triple-redundant stall detection, thermal limits, and isolation moats.

## Current Build Status
- [x] Consolidated Hardware Manager (ISRs, Sensors, Curves).
- [x] PMBus Linear11 Telemetry Decoding.
- [x] OLED Gauge and State Machine UI.
- [x] MOSFET Bypass for Aux Fan (PWM Signal Jumper required on D6).

## Safety Limits
- Max LED Temp: 85°C
- Max PSU Temp: 70°C
- Max Current: 22A (Slew-rate limited)
