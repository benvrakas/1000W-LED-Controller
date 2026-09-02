# Hardware Bill of Materials

Centralized parts list for the 1000W LED Projector Controller. Datasheets for
the linked parts live in [`docs/Hardware/`](../Hardware/).

## Compute & UI

| Component | Part | Notes | Datasheet |
| :--- | :--- | :--- | :--- |
| Microcontroller | Adafruit Feather M4 CAN Express (SAME51J19A) | Native CAN controller, PWM, ADC | [Adafruit Feather M4 Express CAN Pinout.pdf](../Hardware/Adafruit%20Feather%20M4%20Express%20CAN%20Pinout.pdf) |
| Display | Feather Wing OLED, 128x64, I2C | Telemetry, gauges, error display | — |
| Input | Rotary Encoder | Brightness setpoint | — |
| Power Button | Illuminated pushbutton, status LED | Arm/disarm (hold ≥3000ms) | — |
| Signal Isolation | Analog Devices `ADuM1201` (x4) | Digital isolators on all fan/pump PWM & tach lines | See [Pinout_Verification_Report.md](../Pinout_Verification_Report.md) |
| Power Isolation | `SPB09W8-05` isolated DC/DC converter | Bridges primary +24V domain to the isolated MCU-side +5V logic domain | See [Power_Domains.md](Power_Domains.md) |
| Remote ON/OFF Isolation | DC optocoupler, photo-NPN output | Isolates the PSU Remote ON/OFF switching path | See [Power_Domains.md](Power_Domains.md) |
| Regulators | RECOM `R-78E-0.5` (24V→3.3V), `LD1117V` (5V→3.3V) | One per power domain — see [Power_Domains.md](Power_Domains.md) | — |

## Light Engine

| Component | Part | Notes | Datasheet |
| :--- | :--- | :--- | :--- |
| LED Array | Getian High-Density COB LED | Up to 22.0 A; strict thermal limits to prevent lumen degradation | [GETIAN_Product Guide.pdf](../Hardware/GETIAN_Product%20Guide.pdf) |
| Optics | Custom dual-lens collimator (inner glass dome + outer collimating lens) | 3D-printed housing; see [Physical_Build_Features.md](../Physical_Build_Features.md) | — |

## Power

| Component | Part | Notes | Datasheet |
| :--- | :--- | :--- | :--- |
| Primary PSU | Mean Well UHP-1500-48 (base) | 1500 W, 48 V. Blind analog control via `PC` pin (CN71 pin 2) | [UHP-1500-spec.pdf](../Hardware/UHP-1500-spec.pdf) |
| Primary PSU (alt.) | Mean Well UHP-1500-48CAN | Same unit, CAN/PMBus variant — full telemetry + watchdog | [UHP-1500-spec.pdf](../Hardware/UHP-1500-spec.pdf), [PMBus-Specification-Rev-1-3-1-Part-II-20150313.pdf](../Hardware/PMBus-Specification-Rev-1-3-1-Part-II-20150313.pdf), [PMBus_AN001_Rev_1_0_1_20160107.pdf](../Hardware/PMBus_AN001_Rev_1_0_1_20160107.pdf) |
| Secondary PSU | Mean Well **LRS-350-24** | 350W, 24V/14.6A. On the same mains branch as the UHP-1500. Feeds the primary `+24V` domain (fans, pump) — see [Power_Domains.md](Power_Domains.md) | [LRS-350-24-spec.pdf](../Hardware/LRS-350-24-spec.pdf) |
| Superseded spec | — | [OLD uhp1500.pdf](../Hardware/OLD%20uhp1500.pdf) kept for reference only; current unit is the UHP-1500-48 above | [OLD uhp1500.pdf](../Hardware/OLD%20uhp1500.pdf) |

## Cooling (Liquid Loop)

| Component | Part | Notes | Datasheet |
| :--- | :--- | :--- | :--- |
| Water Pump | Xylem/Laing Ecocirc D5 Vario | Shaftless spherical motor. Physical unit has a PWM control wire + tach wire, but [cat_ecocirc_d5vario_uk_web.pdf](../Hardware/cat_ecocirc_d5vario_uk_web.pdf) on file is a sales catalog for the base 2-wire/dial-speed model and doesn't document that interface — PWM timing spec (frequency, logic levels) is unvalidated; get the actual control-interface datasheet if it becomes available. | [cat_ecocirc_d5vario_uk_web.pdf](../Hardware/cat_ecocirc_d5vario_uk_web.pdf) *(incomplete — see note)* |
| Radiator | EK 140x420mm, 50mm thick, 3-fan | Liquid cooling radiator for the LED die loop | — *(no datasheet on file)* |
| Radiator Fans | Delta FFB1424VHG-EP, 140mm PWM | High static-pressure, 3x on the radiator | [FFB1424VHG-EP.pdf](../Hardware/FFB1424VHG-EP.pdf) |
| PSU Fan | Delta FFB1424VHG-EP, 140mm PWM | Cools the PSU, 1x | [FFB1424VHG-EP.pdf](../Hardware/FFB1424VHG-EP.pdf) |
| Auxiliary/Lens Fan | NMB Technologies **12038VA-24Q-EM**, 120mm, 4-wire (PWM + tach) | Cools optical path and projector lenses, 1x. Confirmed on the physical unit's label 2026-08-28 -- earlier docs/firmware comments assumed the -24R part (see note below). | [12038VA data sheet.pdf](../Hardware/12038VA%20data%20sheet.pdf) *(covers the -24R PWM behavior, not Q-class-specific -- see note)* |
| Cooling Block | Direct-die liquid cooling block | Mounted under the LED die; high-temp corrugated tubing | — |

## Sensors

| Component | Part | Notes |
| :--- | :--- | :--- |
| LED Thermistor | 10k NTC, Beta 3950 | 10K+10K voltage divider to ground, monitors LED block temp |
| Water Thermistor | 10k NTC, Beta 3950 | 10K+10K voltage divider to ground, monitors coolant temp |
| Fan/Pump Tachometers | Integrated in each fan/pump | Stall detection (see [System Overview](Firmware_Architecture.md)) |

## Chassis & Structural

See [Physical_Build_Features.md](../Physical_Build_Features.md) for the full breakdown (T-slot aluminum extrusion frame, custom 3D-printed brackets/handle/shrouds, honeycomb exhaust grill).

## Notes

- No datasheet is on file for the EK radiator — add one to `docs/Hardware/` if it becomes available.
- **Aux/lens fan part number correction (2026-08-28):** the physical fan's
  label reads `12038VA-24Q-EM`, not the `-24R` part previously assumed in
  firmware comments and this doc. Both are NMB's 120x38mm `12038VA` PWM fan
  body, differing only in speed grade (Q: 5700 RPM max / 63dB vs R: 6400 RPM
  max / 66dB, per the datasheet on file) -- confirmed via NMB's official
  part-numbering guide (size/thickness/series/bearing prefix decodes
  identically) and multiple independent retail listings that describe
  `-24Q-EM` as 4-wire (power + PWM + tach), matching the `-24R-FU` part's
  wire count. What's *not* confirmed: the `-24R` PWM addendum in the on-file
  datasheet (25kHz PWM frequency, 30% minimum start duty, TACHO circuit) is
  written specifically against the R-class SKU. NMB does not appear to
  publish a distinct PWM behavioral datasheet per speed-class (checked
  nmbtc.com's official part pages, Mouser, DigiKey/Newark, and NMB's own
  part-numbering-system PDF -- none document a Q-class-specific PWM
  frequency), so the firmware's 25kHz/30%-start assumptions ([Tachometers.cpp](../../src/drivers/Tachometers.cpp), [ThermalConfig.h](../../include/config/ThermalConfig.h))
  are carried over from the R-class sheet on the working assumption that
  NMB reuses the same PWM decode circuit across the whole `12038VA` PWM
  family and only the motor/impeller changes per speed class. If the lens
  fan continues to ignore commanded PWM duty after the diagnostic logging
  added to `configureFixedFrequencyPwm()`, get NMB to confirm the `-24Q-EM`
  PWM frequency directly before assuming this is a wiring/firmware bug.
- `docs/Hardware/` is the single home for all component datasheets; do not duplicate PDFs at the `docs/` root.
