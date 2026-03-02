# 1000W LED Controller - System Architecture

## Overview

This firmware controls a high-power (1000W) LED projector using a SAMD51-based Adafruit Feather M4. The architecture follows a layered design with clear separation of concerns.

## Layer Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.cpp                                │
│              (Driver instantiation, setup/loop)                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    State Machine Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  StateInit   │  │   StateRun   │  │   StateErrorKill     │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
│                    SystemController                             │
└─────────────────────────────────────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│  InputService   │  │  PsuController  │  │CoolingController│
│  (encoder,btn)  │  │   (CAN, gate)   │  │ (fans, pump)    │
└─────────────────┘  └─────────────────┘  └─────────────────┘
          │                   │                   │
          ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────────────────┐
│                       Driver Layer                              │
│  EncoderManager │ PowerButtonManager │ CanBusManager │ etc.    │
│  ThermistorManager │ TachometerManager │ OledManager            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Hardware (GPIO, CAN, I2C)                  │
│              Mean Well UHP-1500-48 PSU │ OLED │ Sensors         │
└─────────────────────────────────────────────────────────────────┘
```

## Key Abstractions

### SystemViewModel (`include/core/SystemViewModel.h`)
Read-only snapshot of all system telemetry, built by `StateRun` from service outputs. Passed to:
- `UiController` for display rendering
- `ErrorLogger` for fault record snapshots

This ensures UI and logging have no direct driver dependencies.

### Services

| Service | Location | Responsibility |
|---------|----------|----------------|
| `InputService` | `include/services/InputService.h` | Encapsulates encoder + power button. Exposes `isArmed()`, `getKnobFraction()`, edge events |
| `PsuService` | `include/services/PsuService.h` | Controls Mean Well PSU via CAN + remote gate pin. Slew-limited current ramping |
| `CoolingService` | `include/services/CoolingService.h` | Owns thermistors + tachometers. Returns `CoolingState` with temps and RPMs |

### Drivers

| Driver | Location | Hardware |
|--------|----------|----------|
| `EncoderManager` | `include/drivers/Encoder.h` | 600 PPR rotary encoder for power setpoint |
| `PowerButtonManager` | `include/drivers/PowerButton.h` | Illuminated latching button (3s hold on, tap off) |
| `CanBusManager` | `include/drivers/CanBus.h` | CAN 2.0B interface to Mean Well PSU |
| `TachometerManager` | `include/drivers/Tachometers.h` | PWM + tach for fans/pump with PID control |
| `ThermistorManager` | `include/drivers/Thermistors.h` | NTC thermistors (LED junction, water loop) |
| `OledManager` | `include/drivers/OLED.h` | 128x32 SSD1306 OLED display |

### State Machine

| State | Handler | Description |
|-------|---------|-------------|
| `INIT` | `StateInit.cpp` | Hardware bring-up, cooling deadstart, CAN init |
| `RUN` | `StateRun.cpp` | Normal operation with services |
| `ERROR_KILL` | `StateErrorKill.cpp` | Safe shutdown on fault |

## Data Flow

```
User Input (encoder/button)
         │
         ▼
    InputService ──► PsuController.setUiSetpointFraction()
         │                    │
         │                    ▼
         │           Slew-limited CAN commands to PSU
         │
         ├──► CoolingController.update() ──► Fan/pump PWM
         │
         ▼
    StateRun builds SystemViewModel
         │
         ├──► UiController.update(vm) ──► OLED display
         │
         └──► ErrorLogger.update(state, vm) ──► Fault records
```

## Fault System

- `FaultCode` enum in `include/logging/FaultManager.h`
- `FaultManager::raiseFault()` / `clearFault()` API
- `ErrorLogger` captures `SystemViewModel` snapshot on fault change

## Directory Structure

```
include/
├── core/           # Core types (SystemViewModel)
├── services/       # Domain services (InputService, PsuService, CoolingService)
├── drivers/        # Hardware abstractions
├── logging/        # FaultManager, ErrorLogger
├── util/           # BoardPins, PID, ThermalCurve
├── state/    # State machine (INIT, RUN, ERROR_KILL)
└── ui/             # UiController

src/                # Implementation files (mirrors include/)
```

## Build

```bash
pio run          # Build
pio run -t upload # Flash to Feather M4
```
