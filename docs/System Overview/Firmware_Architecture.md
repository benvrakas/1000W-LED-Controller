# Firmware Architecture

Firmware for a high-power (1000W) LED projector controller based on the
**Adafruit Feather M4 CAN Express (SAME51)**. It manages a **Mean Well
UHP-1500-48** power supply (either the analog-control base model or the
CAN/PMBus variant), controls active liquid cooling, and provides a user
interface via a Button, OLED, and Rotary Encoder.

For the physical parts list, see [Hardware.md](Hardware.md). For pin-level
wiring, see [Electronics_Architecture.md](Electronics_Architecture.md).

## Key Features

* **Power Control**: Two control paths to the **Mean Well UHP-1500-48**, selectable at compile time via `PsuControlConfig::PSU_CONTROL_VIA_CAN` in [PowerConfig.h](../../include/config/PowerConfig.h):
    * **Analog (blind) mode** *(current default)*: PWM → RC low-pass → PSU `PC` pin (CN71 pin 2). No telemetry; PSU on/off still gated by the `Remote ON/OFF` line.
    * **CAN mode**: Native CAN Bus PMBus, with full telemetry, current programming, and a comms watchdog. Used when a UHP-1500-48CAN unit is fitted.
* **Active Cooling**: A single PI (Proportional-Integral) controller targets water/coolant temperature and drives the main radiator fan. The PSU fan and aux/lens fan track the LED's commanded duty with a fixed linear ratio (20% and 50% of LED duty respectively) whenever the LED is on. The pump has no PI loop at all — it's a fixed duty (max while the LED is on, a low idle otherwise).
* **User Interface**: OLED status display (128x64) and Rotary Encoder input for brightness mapping.
* **Safety**:
    * Latched `ERROR_KILL` state for critical faults (Over-temp, Cooling stall, CAN timeout, encoder fault).
    * Hardware enable lines dropped immediately on fault.
    * Per-channel fault ignoring: a 3s button hold in `ERROR_KILL` permanently ignores (until power cycle) only the specific channel that faulted, not all fault checking — see [3.3](#33-fault-protection-error_kill-state).

## 1. Core Architectural Pattern
The software is structured using a **Dependency Injection (DI)** pattern, implemented in C++ with the Arduino/PlatformIO framework. This cleanly separates the hardware driver layer from the higher-level business logic.

### 1.1 Hardware Layer (`src/core/Hardware.cpp`)
*   **Role:** The "Composition Root". Instantiates all physical drivers (Tachometers, CAN backend, Analog PSU backend, OLED, Thermistors) and pin configurations.
*   **Key Components:**
    *   `NativeCanBackend`: Adapts the SAMD51 hardware CAN controller to the `ICanBackend` interface. Brought online only when `PSU_CONTROL_VIA_CAN == true`.
    *   `AnalogPsuBackend`: Drives the UHP-1500-48 base model's `PC` analog input via PWM. Active when `PSU_CONTROL_VIA_CAN == false`.
    *   `TachometerManager`: Handles PWM output and tachometer pulse counting. Defaults to the SAMD51 core's PWM frequency (~1.8kHz), but the aux fan is constructed with an optional fixed-frequency override (25kHz, via direct TC timer register access) sourced from the NMB 12038VA-24R datasheet's exact 25kHz PWM spec ("Vst = Open -> Full Speed" if it can't decode the signal). Per-channel PWM signal requirements, as validated against the datasheets on file:
        *   **Aux fan** (physical part is NMB **12038VA-24Q-EM**, a lower-speed-grade sibling of the -24R the PWM figures below are actually sourced from — no Q-class-specific PWM datasheet exists; see [Hardware.md](Hardware.md) for the full part-number note): 25kHz required — fixed via the `fixedPwmFreqHz` override above (direct `TC3` register config, since this pin doesn't share a timer with any other fan).
        *   **Radiator fans + PSU fan** (Delta FFB1424VHG-EP x4, sharing `TCC1` across pins D5/D6/D9): datasheet specs 50Hz as the preferred PWM frequency, and the same "control line disconnected -> full speed" fail-safe as the NMB fan. The firmware runs this channel at the core default (~1.8kHz) instead — off-spec by ~36x, but confirmed working correctly on real hardware, so left as-is rather than reconfigured. Its "start at >=30% duty from a dead stop" recommendation already matches `TachometerConfig::MAIN_PSU_DEADSTART_DUTY` (~30%).
        *   **Pump** (Xylem/Laing Ecocirc D5 Vario): the datasheet on file (`cat_ecocirc_d5vario_uk_web.pdf`) is a sales catalog for the base 2-wire/dial-speed model and doesn't cover the PWM+tach variant actually installed — no PWM timing spec is available to validate against. `TachometerConfig::PUMP_DEADSTART_DUTY` (~50%) is therefore unvalidated; get the real control-interface datasheet if one becomes available.

### 1.2 AppContext (`include/core/AppContext.h`)
*   **Role:** The "Toolbelt". A struct containing references to all hardware instances.
*   **Purpose:** Passed to the `SystemController` and Services, allowing them to access hardware without relying on global variables.

### 1.3 System Controller (`src/state/SystemController.cpp`)
*   **Role:** The "Brain". Manages the high-level Finite State Machine (FSM) and owns the domain services.
*   **States:**
    1.  **INIT**: Validates sensors, spins up pumps/fans, and checks for hardware faults before enabling high power.
    2.  **RUN**: The main control loop. Updates services, inputs, and UI.
    3.  **ERROR_KILL**: A latched safety state entered upon critical faults (Over-temp, cooling stall, CAN timeout, encoder fault). Disables PSU immediately. Reached both from a `RUN`-time fault and from an `INIT` boot-step timeout — there is only one failure state, not two.

## 2. Domain Services
Logic is divided into isolated services, injected with their specific dependencies.

| Service | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **`PsuService`** | Manages LED current setpoint, slew limiting, and PSU enable/disable logic. Routes commands to either CAN or analog backend based on `PSU_CONTROL_VIA_CAN`. | `CanBusManager`, `AnalogPsuBackend` |
| **`CoolingService`** | Runs a single PI loop (targeting water temperature) that drives the main radiator fan; PSU/aux fans track LED duty by a fixed ratio when the LED is on; the pump runs a fixed duty with no PI loop at all. | `TachometerManager`, `ThermistorManager` |
| **`InputService`** | Processes encoder rotation and power button (arm/disarm) logic. | `EncoderManager`, `PowerButtonManager` |
| **`UiController`** | Renders the system state to the OLED display (Telemetry, Gauges, Errors). | `OledManager` |

## 3. Operational Flow & State Machine

### 3.1 Boot-Up and Initialization (`INIT` State)
When the system is plugged in, it enters the `INIT` state and begins a multi-step verification process to bootstrap the hardware:
1.  **Board Pins:** GPIOs are configured. Crucially, all PWM outputs and the `PSU_REMOTE` pin are forced `LOW` to ensure the system boots in a safe, unpowered state.
2.  **Cooling Priming:** The D5 pump and all fans are commanded to a ~50% "spin-up" duty (`TachometerConfig::*_SPINUP_DUTY`) for `SPINUP_MS` (1000ms), then their tachometers are verified against the stall thresholds. A channel already ignored via the per-channel fault-ignore mechanism (see [3.3](#33-fault-protection-error_kill-state)) is skipped rather than blocking the boot step.
3.  **PSU Init:** The Mean Well CAN interface is initialized, but the remote ON/OFF gate remains **disabled** (`LOW`).
4.  **Display:** The OLED boots and shows the startup screen.
5.  **Transition:** Once all steps pass, the system transitions into the `RUN` state. If any step times out (taking longer than `TachometerConfig::BOOT_STEP_TIMEOUT_MS`, 2000ms), the system aborts to the `ERROR_KILL` state.

### 3.2 Normal Operation (`RUN` State)
In the `RUN` state, the main operational loop executes.

**Active Cooling Control:**
`CoolingService::update()` takes the LED's actual applied duty fraction (`PsuService::getAppliedCurrentFraction()`) each tick and drives four channels with different policies — only one of which is still a PI loop:
*   **Main radiator fan (PI):** The only remaining PI controller. Targets **40.0°C** water/coolant temperature (`FanCurveConfig::TARGET_TEMP_WATER`) — conceptually the radiator's job is dissipating heat the coolant already picked up from the LED block, so it tracks coolant temperature, not the LED's. Integrator is capped to prevent windup, added on top of `MAIN_DUTY_MIN`.
*   **PSU fan & aux/lens fan (fixed linear ratio):** Whenever the LED is on, these track the LED's commanded duty directly rather than any temperature — PSU fan at 20% of LED duty, aux/lens fan at 50% (`FanCurveConfig::PSU_FAN_TO_LED_DUTY_RATIO` / `AUX_FAN_TO_LED_DUTY_RATIO`). When the LED is off, the PSU fan mirrors the main fan's water-PI duty, and the aux fan falls back to a floor duty that boosts alongside the water-PI output (it must keep protecting the lens even off-LED, independent of thermistor validity).
*   **Pump (fixed constant, no PI):** `FanCurveConfig::PUMP_DUTY_MAX` any time the LED is on (unconditional — it must run flat out the moment the LED is drawing power, not react to temperature), `PUMP_DUTY_MIN` when the LED is off. The pump previously ran its own PI loop targeting LED temperature; that loop was removed entirely as unnecessary complexity (the pump is quiet even at full speed, so there was no benefit to throttling it).
*   The PSU fan's tachometer is not currently trusted — `TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED = false` disables its stall check (INIT and runtime) while its PWM is still driven normally.

**Power Control & The Button:**
By default, even in the `RUN` state, the LED array is OFF and the PSU remote gate remains disabled. The virtual brightness knob is initialized at 0%.
To power on the LED array, the user must **press and hold the power button for ≥3000ms**.
*   **Arming:** Once held for 3 seconds, the system is marked as "Armed".
*   **Power On Sequence:** `PsuService` begins driving the current setpoint immediately, then sets the PSU Remote gate `HIGH`. In analog/blind mode (current default) this is deliberately delayed by `AnalogPsuConfig::PC_SETTLE_MS` (50ms) after the PC pin starts changing, so the external RC filter has physically settled past the PSU's undefined sub-0.4V zone before the PSU samples it — asserting Remote too early was a confirmed source of a brief full-power flash on arm. In CAN mode, Remote is instead gated on telemetry validity and no PSU fault being latched, and a PMBus command additionally enables output.
*   **Slew Rate Control:** The requested LED current smoothly ramps up to the target value (set by the encoder knob) at a controlled rate of **30% per second**. This prevents inrush currents and thermal shock to the LED array.
*   **Power Off Sequence:** Pressing the button again "Disarms" the system. The current setpoint slews down at a faster rate of **100% per second**. *Only when the applied current reaches ≤1%* does the controller physically drop the `PSU_REMOTE` gate to `LOW` and disable CAN output. A hard timeout of 3000ms is enforced during this disarm phase: if CAN communication faults or hangs during this exact slew-down window, the firmware unconditionally drops the `PSU_REMOTE` gate to `LOW`, guaranteeing hardware shutoff.

### 3.3 Fault Protection (`ERROR_KILL` State)
Due to the extreme thermal output of a 1000W LED, safety is paramount. The system continuously polls for critical faults. If a fault is detected, the `FaultManager` immediately transitions the system into the latched `ERROR_KILL` state.

**Critical Cut-Offs:**
*   **LED Over-Temperature:** Triggers if LED thermistor reads **> 75.0°C**.
*   **Water Over-Temperature:** Triggers if the liquid cooling loop reaches **> 50.0°C**.
*   **Cooling Stalls:** Fans and pumps have tachometer feedback which is continuously polled in the `RUN` state. If a PWM duty is commanded but the RPM falls below the stall thresholds (whether the LED is armed or not), the system throws a precise `COOLING_FAILURE` fault:
    *   Radiator/PSU Fan Stall: **< 300 RPM**
    *   Auxiliary Fan Stall: **< 300 RPM**
    *   Pump Stall: **< 150 RPM**
*   **CAN Timeout** *(CAN mode only)*: If the PSU stops responding to CAN telemetry requests for **> 1000ms**. In analog/blind mode this check is disabled — the PSU has no feedback path, so the only PSU-side cutoffs are the `Remote ON/OFF` line and the thermal/cooling faults above.

**When `ERROR_KILL` is triggered:**
1.  **Immediate Disconnect:** The `psu.setOperation(false)` command is fired, which drops the `PSU_REMOTE` gate to `LOW` (hardware disable) and sends a CAN disable command.
2.  **Latch:** The system halts normal operation and shows a diagnostics screen (live RPM/duty per channel, plus each channel's peak RPM reached since it last started spinning).

**Per-Channel Fault Ignoring:**
Holding the power button for 3s while latched in `ERROR_KILL` does **not** disable fault checking globally. Instead, `identifyFaultSource()` (`FaultManager.h`/`.cpp`) classifies the *specific* fault currently active into one of eight categories — LED temp, water temp, pump, main fan, PSU fan, aux fan, PSU comms (CAN timeout/PSU fault), encoder — and permanently sets that one channel's ignore flag (`SystemController::global*Ignored`), clears the fault, and returns to `INIT`. Every other channel remains fully monitored. Ignore flags are cleared only by a physical power cycle. Two boot failures aren't mapped to any of the eight categories and so can't be cleared this way — a step-1 (board pins) INIT failure and the `default`/corrupted-boot-step case — the 3s hold does nothing for these and the system stays latched.

Any channel ignored is reflected everywhere: its diagnostics page shows `IGNORED` instead of a pass/fail verdict, the RUN screen's status line shows `[N IGNORED]`, and the NeoPixel goes solid amber (`NeoPixelState::WARNING`) instead of green while armed.

See also [Error_Codes.md](../Error_Codes.md) for the NeoPixel fault-blink patterns emitted in this state.

## 4. Directory Structure
*   `src/` - Implementation files.
    *   `core/` - Hardware instantiation (`Hardware.cpp`) and dependency injection (`AppContext`).
    *   `services/` - Business logic (`CoolingService`, `PsuService`).
    *   `drivers/` - Hardware abstraction (`NativeCanBackend`, `AnalogPsuBackend`, `OLED`).
    *   `state/` - Finite State Machine (`Init`, `Run`, `Error`).
*   `include/config/` - System configuration.
    *   `PinMap.h` - Hardware pin definitions (PWM, Tach, I2C, etc. CAN uses internal board pins).
    *   `ThermalConfig.h` - Thermistor beta values, Fan/Pump RPM limits, spin-up/stall timing, and the water-temp PI gains (`FanCurveConfig`) that now drive only the main radiator fan, plus the fixed PSU/aux-fan-to-LED-duty ratios and pump duty constants.
    *   `PowerConfig.h` - PSU control mode flag, analog control parameters, PSU voltage/current limits (including the overclocked `MAX_LED_CURRENT_A`), CAN bus bitrate, and Slew rates.
*   `docs/` - Documentation.
*   Bench-test firmwares (`src/bench/`, each its own PlatformIO environment in `platformio.ini`) bypass the full state machine to isolate hardware-vs-firmware questions: `aux_fan_bench` (encoder-controlled PWM frequency sweep for the aux/lens fan) and `all_tach_bench` (raw pulse-count isolation test for all four tachometer channels, no PWM driven).

## 5. Building & Flashing
This project uses **PlatformIO**.

**Note for AI Agents / CLI Users:**
If `pio` is not in your PATH, use the full path: `C:\Users\benvr\.platformio\penv\Scripts\pio.exe` (or add it to your PATH).

1.  **Build**: `pio run`
2.  **Upload**: `pio run -t upload`
3.  **Monitor**: `pio device monitor`

## License
Proprietary / Internal Use.
