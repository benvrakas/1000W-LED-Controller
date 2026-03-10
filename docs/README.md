# 1000W LED Projector Controller Firmware (v2.0)

Firmware for a high-power (1000W) LED projector controller based on the **Adafruit Feather M4 CAN Express (SAME51)**. It manages a **Mean Well UHP-1500-48** power supply via CAN Bus, controls active liquid cooling, and provides a user interface via a Button, OLED, and Rotary Encoder.

## 1. System Overview
This system manages the power and cooling of a high-power COB LED array. It ensures safe operation through rigorous state management, PI-based thermal monitoring, and active fault protection.

### Key Features
*   **Power Control**: Digital control of **Mean Well UHP-1500-48** PSU via Native CAN Bus (PMBus protocol). Includes programmable output voltage (50~120%) and constant current limits (20~100%).
*   **Active Cooling**: PI (Proportional-Integral) controllers dynamically adjust Radiator Fans, Pump, and Aux Fans to strictly maintain target temperatures.
*   **User Interface**: OLED status display (128x32) and Rotary Encoder input for brightness mapping.
*   **Safety**:
    *   Latched `ERROR_KILL` state for critical faults (Over-temp, Pump failure, CAN timeout).
    *   Hardware enable lines dropped immediately on fault.
    *   **QSPI Error Logging**: Crashes and faults (along with full system telemetry snapshots) are saved to onboard flash (`error_log.csv`) via a FAT filesystem for post-mortem analysis.

## 2. Hardware Components
The system manages the following specific high-performance components:
*   **Microcontroller:** Adafruit Feather M4 CAN Express (SAME51J19A).
*   **LED Array:** **Getian High-Density COB LED**. These specialized emitters produce intense localized heat and require precise power limits (up to 22.0A) and strict thermal management to prevent lumen degradation.
*   **Power Supply:** **Mean Well UHP-1500-48**. A 1500W, 48V industrial power supply. It is controlled via the PMBus protocol over the native CAN bus, allowing the microcontroller to set exact current limits and slew rates digitally.
*   **Cooling System (Liquid Loop):**
    *   **Water Pump:** **Xylem/Laing Ecocirc D5 Vario**. A highly reliable, shaftless spherical motor pump controlled via PWM with tachometer feedback.
    *   **Radiator Fans:** High static pressure 120mm PWM fans (e.g., NMB Technologies 12038VA series) to force air through the liquid cooling radiator.
    *   **Auxiliary/Lens Fans:** Additional PWM fans (e.g., Delta FFB series) to cool the optical path and projector lenses.
*   **Sensors:** Two 10k NTC Thermistors (Beta: 3950) for monitoring both the LED block temperature and the circulating Water temperature.
*   **Storage:** 2MB QSPI Flash (GD25Q16).

For a complete pinout mapping, refer to [docs/Pinout.md](docs/Pinout.md).

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
2.  **Cooling Priming:** The D5 pump and all fans are initialized and commanded to a minimum "deadstart" duty cycle to prime the liquid cooling loop, clear air bubbles, and establish baseline airflow. (Note: The `INIT` sequence primes the loop immediately without waiting for the fans/pump to physically spin up to avoid unnecessarily blocking the boot sequence. RPM is instead continuously monitored in the `RUN` state.)
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
By default, even in the `RUN` state, the LED array is OFF and the PSU remote gate remains disabled. The virtual brightness knob is initialized at 0%. 
To power on the LED array, the user must **press and hold the power button for $\ge$ 3000ms**. 
*   **Arming:** Once held for 3 seconds, the system is marked as "Armed".
*   **Power On Sequence:** The `PsuService` sets the PSU Remote gate `HIGH` (physically enabling the Mean Well PSU) and sends a PMBus CAN command to enable output.
*   **Slew Rate Control:** The requested LED current smoothly ramps up to the target value (set by the encoder knob) at a controlled rate of **30% per second**. This prevents inrush currents and thermal shock to the LED array.
*   **Power Off Sequence:** Pressing the button again "Disarms" the system. The current setpoint slews down at a faster rate of **100% per second**. *Only when the applied current reaches $\le$ 1%* does the controller physically drop the `PSU_REMOTE` gate to `LOW` and disable CAN output. A hard timeout of 3000ms is enforced during this disarm phase: if CAN communication faults or hangs during this exact slew-down window, the firmware unconditionally drops the `PSU_REMOTE` gate to `LOW`, guaranteeing hardware shutoff.

### 5.3 Fault Protection (`ERROR_KILL` State)
Due to the extreme thermal output of a 1000W LED, safety is paramount. The system continuously polls for critical faults. If a fault is detected, the `FaultManager` immediately transitions the system into the latched `ERROR_KILL` state.

**Critical Cut-Offs:**
*   **LED Over-Temperature:** Triggers if LED thermistor reads **> 75.0°C**.
*   **Water Over-Temperature:** Triggers if the liquid cooling loop reaches **> 50.0°C**.
*   **Cooling Stalls:** Fans and pumps have tachometer feedback which is continuously polled in the `RUN` state. If a PWM duty is commanded but the RPM falls below the stall thresholds (whether the LED is armed or not), the system throws a precise `COOLING_FAILURE` fault:
    *   Radiator/PSU Fan Stall: **< 300 RPM**
    *   Auxiliary Fan Stall: **< 300 RPM**
    *   Pump Stall: **< 150 RPM**
*   **CAN Timeout:** If the PSU stops responding to CAN telemetry requests for **> 500ms**.

**When `ERROR_KILL` is triggered:**
1.  **Immediate Disconnect:** The `psu.setOperation(false)` command is fired, which drops the `PSU_REMOTE` gate to `LOW` (hardware disable) and sends a CAN disable command.
2.  **Latch:** The system halts all normal operations. It will not restart or accept input until physically unplugged and reset.
3.  **Crash Logging:** A final `SystemViewModel` telemetry snapshot is explicitly logged to flash.

## 6. Directory Structure
*   `src/` - Implementation files.
    *   `core/` - Hardware instantiation (`Hardware.cpp`) and dependency injection (`AppContext`).
    *   `services/` - Business logic (`CoolingService`, `PsuService`).
    *   `drivers/` - Hardware abstraction (`NativeCanBackend`, `OLED`).
    *   `state/` - Finite State Machine (`Init`, `Run`, `Error`).
*   `include/config/` - System configuration.
    *   `PinMap.h` - Hardware pin definitions (PWM, Tach, I2C, etc. CAN uses internal board pins).
    *   `ThermalConfig.h` - Thermistor beta values, Fan/Pump RPM limits, and **PI Controller Configuration** (Target temps, Kp, Ki).
    *   `PowerConfig.h` - PSU voltage/current limits, CAN bus bitrate, and Slew rates.
*   `docs/` - Documentation.

## 7. Protocols
*   **CAN Bus**: Uses 29-bit Extended IDs.
    *   **Current Control**: Uses PMBus Linear11 encoding. 100% LED Power (22A) is scaled relative to the PSU Max (31.3A) to ensure correct current command.

## 8. Building & Flashing
This project uses **PlatformIO**.

**Note for AI Agents / CLI Users:**
If `pio` is not in your PATH, use the full path: `C:\Users\benvr\.platformio\penv\Scripts\pio.exe` (or add it to your PATH).

1.  **Build**: `pio run`
2.  **Upload**: `pio run -t upload`
3.  **Monitor**: `pio device monitor`

## License
Proprietary / Internal Use.
