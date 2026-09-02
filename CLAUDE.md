# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a 1000W LED projector controller built on an **Adafruit Feather
M4 CAN Express (SAME51)**. It drives a Mean Well **UHP-1500-48** PSU (analog
or CAN/PMBus variant), runs active liquid cooling via PI controllers, and
exposes a Button/OLED/Rotary-Encoder UI. This is a real, high-power hardware
project (1000W LED, liquid cooling) — treat safety-critical logic (fault
detection, `ERROR_KILL`, slew limiting, PSU gating) with extra care; a bug
here can cause thermal or electrical damage, not just a bad build.

## Build / flash / monitor

This project uses **PlatformIO** (`platformio.ini`, single env: `master`,
board `adafruit_feather_m4_can`, framework `arduino`).

If `pio` isn't on PATH, use the full path:
`C:\Users\benvr\.platformio\penv\Scripts\pio.exe`

- Build: `pio run`
- Upload: `pio run -t upload`
- Serial monitor: `pio device monitor` (115200 baud)

There is no unit test suite currently implemented — `test/` only contains
PlatformIO's default placeholder.

## Architecture

Full detail lives in `docs/System Overview/Firmware_Architecture.md` — read
it before making non-trivial changes. Summary:

- **Dependency injection, no globals-by-reference pattern.** `src/core/Hardware.cpp`
  is the composition root: it instantiates every physical driver
  (tachometers, CAN backend, analog PSU backend, OLED, thermistors, etc.) as
  globals, then `main.cpp` wires references to them into a single
  `AppContext` struct (`include/core/AppContext.h`). `AppContext` is passed
  into `SystemController` and all services — that's how they reach hardware
  instead of touching globals directly.
- **Finite state machine** (`src/state/SystemController.cpp`,
  `include/state/SystemController.h`) drives everything from `main.cpp`'s
  `loop()`. States: `INIT` → `RUN` → `ERROR_KILL` (latched; requires physical
  reset). Each state's handler lives in its own file under `src/state/`
  (`StateInit.cpp`, `StateRun.cpp`, `StateErrorKill.cpp`).
- **Domain services** (`src/services/`), each owned by `SystemController` and
  injected with only the hardware they need via `AppContext`:
  `PsuService` (current setpoint, slew limiting, CAN-vs-analog routing),
  `CoolingService` (PI control for fans/pump), `InputService` (encoder +
  power button), `UiController` (OLED rendering, lives in `src/ui/`).
- **Drivers** (`src/drivers/`) are the hardware abstraction layer —
  `AnalogPsuBackend` (PWM→RC→PSU `PC` pin) and `NativeCanBackend` are
  alternate backends selected by `PsuControlConfig::PSU_CONTROL_VIA_CAN` in
  `include/config/PowerConfig.h`; both are always compiled in so the flag can
  flip without code changes.
- **Config headers** (`include/config/`) are the knobs: `PinMap.h` (pin
  assignments), `PowerConfig.h` (PSU control mode, current/voltage limits,
  slew rates, CAN bitrate), `ThermalConfig.h` (thermistor beta values, PI
  gains, target temps, stall thresholds). Prefer editing these over hardcoding
  values in logic.
- **`src/logging/FaultManager.cpp`** owns fault detection/latching that
  forces `ERROR_KILL`; `docs/Error_Codes.md` documents the NeoPixel
  blink-code encoding it drives.

## Hardware/electrical context

- `docs/Pinout.md` — MCU pin ↔ signal table (source of truth; keep in sync
  with `include/config/PinMap.h`).
- `docs/System Overview/Electronics_Architecture.md` — PSU control interfaces
  (CAN vs analog `PC`/`PV`), transfer curves.
- `docs/System Overview/Power_Domains.md` — the three electrically isolated
  ground domains (primary `GND`, isolated `ISO_GND`, PSU-referenced
  `GND-AUX`/`GND-signal`) and the specific components bridging them. Read
  this before proposing any new ground connection — domains are isolated on
  purpose (galvanic isolation, ground-loop avoidance), and bridging the wrong
  two will corrupt the current setpoint or defeat isolation.
- `docs/System Overview/PCB_TODO.md` — outstanding hardware work, notably the
  CN71 analog control interface (`PC`/`PV`) which is not yet built on the PCB.
- `docs/System Overview/Hardware.md` — physical parts list.
- KiCad sources live under `docs/Kicad/Temp Snapshot/` (schematic/PCB/exports).

## Firmware conventions

- Two PSU control paths are both always compiled in and selected by a single
  compile-time flag (`PsuControlConfig::PSU_CONTROL_VIA_CAN`) — don't
  `#ifdef` one out or assume only one path exists.
- Safety cutoffs (over-temp, cooling stall, CAN timeout) are checked
  continuously in `RUN` regardless of whether the LED is armed; don't gate
  them behind the arm/disarm state.
- `SystemController` stores one `InitData`/`RunData`/`ErrorKillData` struct
  at a time (state-specific, wiped on transition) plus persistent `global*`
  fields available across all states — check `SystemController.h` before
  adding new per-state vs. cross-state fields.
