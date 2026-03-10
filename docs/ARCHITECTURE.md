# ProjectorMaster Architecture v2.0

## 1. System Overview
Firmware for a high-power (1000W) LED projector controller based on the **Adafruit Feather M4 CAN Express (SAME51)**. It manages a **Mean Well UHP-1500-48** power supply via CAN Bus, controls active liquid cooling, and provides a user interface via a Button, OLED, and Rotary Encoder.

## 2. Hardware Components
The system manages the following specific high-performance components:
*   **Microcontroller:** Adafruit Feather M4 CAN Express (SAME51)
*   **LED Array:** **Getian High-Density COB LED**. These specialized emitters produce intense localized heat and require precise power limits (up to 22.0A) and strict thermal management to prevent lumen degradation.
*   **Power Supply:** **Mean Well UHP-1500-48**. A 1500W, 48V industrial power supply. It is controlled via the PMBus protocol over the native CAN bus, allowing the microcontroller to set exact current limits and slew rates digitally.
*   **Cooling System (Liquid Loop):**
    *   **Water Pump:** **Xylem/Laing Ecocirc D5 Vario**. A highly reliable, shaftless spherical motor pump controlled via PWM with tachometer feedback.
    *   **Radiator Fans:** High static pressure 120mm PWM fans (e.g., NMB Technologies 12038VA series) to force air through the liquid cooling radiator.
    *   **Auxiliary/Lens Fans:** Additional PWM fans (e.g., Delta FFB series) to cool the optical path and projector lenses.
*   **Sensors:** Two 10k NTC Thermistors (Beta: 3950) for monitoring both the LED block temperature and the circulating Water temperature.
*   **User Interface:** A 128x32 I2C OLED display for status telemetry, paired with a Rotary Encoder and an illuminated Power Button for user input.
*   **Storage:** 2MB onboard QSPI flash used for non-volatile error logging.

## 3. Core Architectural Pattern
The software is structured using a **Dependency Injection (DI)** pattern, implemented in C++ with the Arduino/PlatformIO framework. This cleanly separates the hardware driver layer from the higher-level business logic.

### 3.1 Hardware Layer (`src/core/Hardware.cpp`)
*   **Role:** The "Composition Root". Instantiates all physical drivers (Tachometers, CAN backend, OLED, Thermistors) and pin configurations.
*   **Key Components:**
    *   `NativeCanBackend`: Adapts the SAMD51 hardware CAN controller to the `ICanBackend` interface.
    *   `TachometerManager`: Handles PWM output and tachometer pulse counting.

### 3.2 AppContext (`include/core/AppContext.h`)
*   **Role:** The "Toolbelt". A struct containing references to all hardware instances.
*   **Purpose:** Passed to the `SystemController` and Services, allowing them to access hardware without relying on global variables.

### 3.3 System Controller (`src/state/SystemController.cpp`)
*   **Role:** The "Brain". Manages the high-level Finite State Machine (FSM) and owns the domain services.
*   **States:**
    1.  **INIT**: Validates sensors, spins up pumps/fans, and checks for hardware faults before enabling high power.
    2.  **RUN**: The main control loop. Updates services, inputs, and UI.
    3.  **ERROR_KILL**: A latched safety state entered upon critical faults (Over-temp, CAN timeout). Disables PSU immediately.

## 4. Domain Services
Logic is divided into isolated services, injected with their specific dependencies.

| Service | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **`PsuService`** | Manages LED current setpoint, slew limiting, and PSU enable/disable logic. | `CanBusManager` |
| **`CoolingService`** | Runs PI controllers to maintain target temperatures for the LED block and water loop. | `TachometerManager`, `ThermistorManager` |
| **`InputService`** | Processes encoder rotation and power button (arm/disarm) logic. | `EncoderManager`, `PowerButtonManager` |
| **`UiController`** | Renders the system state to the OLED display (Telemetry, Gauges, Errors). | `OledManager` |

## 5. Operational Flow & State Machine

### 5.1 Boot-Up and Initialization (`INIT` State)
When the system is plugged in, it enters the `INIT` state and begins a multi-step verification process to bootstrap the hardware:
1.  **Board Pins:** GPIOs are configured. Crucially, all PWM outputs and the `PSU_REMOTE` pin are forced `LOW` to ensure the system boots in a safe, unpowered state.
2.  **Cooling Priming:** The D5 pump and all fans are initialized and commanded to a minimum "deadstart" duty cycle to prime the liquid cooling loop, clear air bubbles, and establish baseline airflow.
3.  **PSU Init:** The Mean Well CAN interface is initialized, but the remote ON/OFF gate remains **disabled** (`LOW`).
4.  **Display:** The OLED boots and shows the startup screen.
5.  **Transition:** Once all steps pass, the system transitions into the `RUN` state. If any step times out (taking longer than 500ms), the system aborts to the `ERROR_KILL` state.

### 5.2 Normal Operation (`RUN` State)
In the `RUN` state, the main operational loop executes.

**Active PI Cooling Control:**
The `CoolingService` runs two separate Proportional-Integral (PI) controllers to maintain optimal temperatures:
*   **LED Loop (Fans):** Targets exactly **55.0°C** at the LED block. The PI controller dynamically adjusts the radiator and aux fans to find the exact equilibrium RPM needed to hold this temperature, protecting the Getian COB from thermal degradation.
*   **Water Loop (Pump):** Targets **40.0°C** for the circulating liquid. The pump's PI controller runs with softer tuning parameters to account for the high thermal mass of the liquid loop.
*   *Anti-Windup & Safety:* The integrators are capped to prevent windup, and are added on top of the "deadstart" minimum duties to ensure the fans and pump never stall during operation.

**Power Control & The Button:**
By default, even in the `RUN` state, the LED array is OFF and the PSU remote gate remains disabled. 
To power on the LED array, the user must **press and hold the power button for $\ge$ 3000ms**. 
*   **Arming:** Once held for 3 seconds, the system is marked as "Armed".
*   **Power On Sequence:** The `PsuService` sets the PSU Remote gate `HIGH` (physically enabling the Mean Well PSU) and sends a PMBus CAN command to enable output.
*   **Slew Rate Control:** The requested LED current smoothly ramps up to the target value at a controlled rate of **30% per second**. This prevents inrush currents and thermal shock to the LED array.
*   **Power Off Sequence:** Pressing the button again "Disarms" the system. The current setpoint slews down at a faster rate of **100% per second**. *Only when the applied current reaches $\le$ 1%* does the controller physically drop the `PSU_REMOTE` gate to `LOW` and disable CAN output.

### 5.3 Fault Protection (`ERROR_KILL` State)
Due to the extreme thermal output of a 1000W LED, safety is paramount. The system continuously polls for critical faults. If a fault is detected, the `FaultManager` immediately transitions the system into the latched `ERROR_KILL` state.

**Critical Cut-Offs:**
*   **LED Over-Temperature:** Triggers if LED thermistor reads **> 75.0°C**.
*   **Water Over-Temperature:** Triggers if the liquid cooling loop reaches **> 50.0°C**.
*   **Cooling Stalls:** Fans and pumps have tachometer feedback. If a PWM duty is commanded but the RPM falls below the stall thresholds, it faults:
    *   Radiator/PSU Fan Stall: **< 300 RPM**
    *   Auxiliary Fan Stall: **< 300 RPM**
    *   Pump Stall: **< 150 RPM**
*   **CAN Timeout:** If the PSU stops responding to CAN telemetry requests for **> 500ms**.

**When `ERROR_KILL` is triggered:**
1.  **Immediate Disconnect:** The `psu.setOperation(false)` command is fired, which drops the `PSU_REMOTE` gate to `LOW` (hardware disable) and sends a CAN disable command.
2.  **Latch:** The system halts all normal operations. It will not restart or accept input until physically unplugged and reset.

## 6. Configuration & Logging
All system tuning parameters are centralized in `include/config/`:
*   **`PinMap.h`**: Hardware pin definitions (PWM, Tach, I2C, etc. CAN uses internal board pins).
*   **`ThermalConfig.h`**: Thermistor beta values, Fan/Pump RPM limits, and **PI Controller Configuration** (Target temps, Kp, Ki).
*   **`PowerConfig.h`**: PSU voltage/current limits, CAN bus bitrate, and Slew rates.

**Crash Logging:**
When `ERROR_KILL` is triggered, the `ErrorLogger` writes the exact fault code, timestamp, and system state to an onboard `error_log.csv` file residing on the 2MB QSPI Flash chip (formatted as a FAT filesystem). This ensures post-mortem diagnostics survive power loss.

## 7. Protocols
*   **CAN Bus**: Uses 29-bit Extended IDs.
    *   **Current Control**: Uses PMBus Linear11 encoding. 100% LED Power (22A) is scaled relative to the PSU Max (31.3A) to ensure correct current command.

---

## Architectural Notes & Potential Improvements
While the current architecture is highly robust, reviewing the code reveals a few areas where the system design could be improved in future iterations:

1.  **Hard Timeout on Shutdown Slew:**
    When the user turns the system off, the `PSU_REMOTE` gate is kept `HIGH` until the current smoothly slews down to 1%. However, if a CAN communication fault occurs during this exact slew-down window, or the PSU hardware ignores the command, the current will never reach 1%, and the remote gate will remain `HIGH` indefinitely. Adding a hard timeout (e.g., dropping the gate unconditionally after 3 seconds) during the disarm phase would guarantee hardware shutoff.
2.  **Strict RPM Verification During Boot:**
    The `StateInit` sequence primes the pump and fans but currently assumes the initialization succeeded instantly (`// For now, assume init succeeded`). Implementing a blocking check that waits for the actual tachometer interrupts to report `RPM > STALL_RPM` before advancing to the `RUN` state would mathematically guarantee the cooling loop is physically functional before high-voltage systems are allowed to arm.
