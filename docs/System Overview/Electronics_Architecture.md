# Electronics Architecture

Pin-level wiring and PSU control interfaces for the 1000W LED Projector
Controller. For the full pin table see [Pinout.md](../Pinout.md); for
cross-checks against the schematic/PCB see
[Pinout_Verification_Report.md](../Pinout_Verification_Report.md). For the
component list, see [Hardware.md](Hardware.md).

## 1. Pinout Summary

Summary of PSU-relevant lines (see [Pinout.md](../Pinout.md) for the complete pin assignment table, including cooling, sensor, and display pins):

| Pin | MCU GPIO | Function | Active When |
| :--- | :--- | :--- | :--- |
| A4 | PA04 | PSU Remote ON/OFF | Both modes |
| A5 | PA06 | PSU PC (analog current setpoint, PWM → RC) | `PSU_CONTROL_VIA_CAN == false` |
| CAN (internal) | — | PMBus over CAN bus | `PSU_CONTROL_VIA_CAN == true` |

All fan/pump PWM and tachometer lines pass through `ADuM1201` digital
isolators (see [Pinout_Verification_Report.md](../Pinout_Verification_Report.md)).
The PSU Remote ON/OFF line is isolated separately via an optocoupler. For the
full breakdown of power/ground domains and every isolation component, see
[Power_Domains.md](Power_Domains.md).

## 2. PSU Control Interfaces

### 2.1 CAN Mode (UHP-1500-48CAN)
*   **CAN Bus**: Uses 29-bit Extended IDs.
    *   **Current Control**: Uses PMBus Linear11 encoding. 100% LED Power (`PsuConfig::MAX_LED_CURRENT_A`, currently 24.2A — a deliberate 110% overclock of the 22.0A nominal rating) is scaled relative to the PSU Max (31.3A) to ensure correct current command. CAN mode isn't rail-limited (digital command), so it can actually reach the full 24.2A — unlike analog mode below.

### 2.2 Analog/Blind Mode (UHP-1500-48 base)

The base UHP-1500-48 has no digital bus. Instead, CN71 exposes two analog programming pins referenced to the PSU's `−V` output:

| CN71 Pin | Function | Range |
| :--- | :--- | :--- |
| 1 | `PV` (programmable voltage) | 0–4.8 V → 50–120 % rated Vout |
| 2 | `PC` (programmable current) | 0.4 V → 20 %, 4.7 V → 100 % rated Iout (linear) |

Only `PC` is driven by this firmware (`PIN_PSU_PC_PWM` = A5). The other CN71 lines used are:

| CN71 Pin | Function | Driven By |
| :--- | :--- | :--- |
| 5 | Remote ON/OFF | `PIN_PSU_REMOTE` (A4) — short to +12 V-AUX = ON |
| 6 | DC-OK (5 V TTL) | *Not wired in firmware yet — reserved on `MISO`* |
| 7, 8 | +12 V-AUX | PSU-sourced auxiliary, drives the Remote loop |

**Transfer curve (PC → output current):**

The SAMD51 GPIO swings 0–3.3 V; through a passive RC into the high-impedance PC pin it lands within ~50 mV of rail. Mapping:

| PWM duty (8-bit) | Filtered V on PC | PSU output | LED current |
| ---: | ---: | ---: | ---: |
| 0 (0 %) | 0 V | off (non-linear region) | — |
| 39 (≈15 %) | ~0.5 V | ~22 % rated (armed floor, `PC_VOLTAGE_MIN_ARMED`) | ~6.8 A |
| 255 (100 %) | 3.30 V (rail-clamped) | ~74 % rated | **~23.15 A (analog-mode ceiling)** |

`PsuConfig::MAX_LED_CURRENT_A` is configured as **24.2 A** (a deliberate 110% overclock of the 22.0A nominal rating — see [PowerConfig.h](../../include/config/PowerConfig.h)), and `AnalogPsuBackend` derives the PC voltage that number would require at runtime. In analog/blind mode that voltage (~3.48 V) exceeds what the 3.3 V MCU rail can drive through the RC filter, so `writePwmForFraction()` clamps to the rail — **analog mode physically caps out around ~23.15 A (~105% of nominal), not the full 24.2 A**, for roughly the top ~6% of the knob's range (everything above ~94% UI setpoint lands on the same rail-clamped duty). This is a real, currently-accepted headroom limit, not a bug to fix — `MAX_LED_CURRENT_A` is still fully reachable in **CAN mode**, which commands current digitally and isn't rail-limited. One consequence worth knowing: `AnalogPsuBackend::getCommandedCurrentA()` (used for the UI's displayed current in blind mode, since there's no real telemetry) computes its number from the *unclamped* target and so can report up to 24.2 A at full knob even though the PC pin itself is capped at the ~23.15 A duty — the display and the physical output diverge slightly at the very top of the range. Below the floor voltage the PSU can't track (datasheet's ~20% floor); the `RUN`-state slew + Remote-line dropout handles the "off" case by physically gating the PSU when applied current ≤ 1 %.

**External hardware required:**
*   RC low-pass between A5 and PC: 1 kΩ resistor + 10 µF ceramic (X7R, ≥6.3 V — `PC` only sees 0–3.3 V so no electrolytic needed), ~16 Hz corner — well below the PWM frequency and well above the slew rate.
*   The PC pin is non-isolated, referenced to the PSU's `GND-signal` (CN71 pins 3/4), which is bonded to `−V`. Bond this board's `ISO_GND` to CN71 pins 3/4 (not to the `TB3 -Vo` power terminal — that carries the full LED return current and will corrupt the setpoint via IR drop). The RC filter's ground return is the designated bond point — land the capacitor's ground leg directly on the `GND-signal` wire, and make sure it's the *only* path tying `ISO_GND` to `GND-signal` (no other stray connection), keeping that return short and direct since it's the actual reference the PSU measures `PC` against. See [Power_Domains.md](Power_Domains.md) for why this doesn't create a ground loop with the primary +24V supply.

**What is lost vs. CAN mode:**
*   No real voltage / current measurements — UI shows the *commanded* current and the nominal 48 V rail. `[ON-BLIND]` is shown in place of `[POWER ON]`.
*   No `CAN_TIMEOUT` fault — `Remote ON/OFF` is the only PSU-side kill path. Thermal and cooling faults are unaffected.
*   No PSU-reported internal faults (`PSU_FAULT` code is unreachable in this mode).
