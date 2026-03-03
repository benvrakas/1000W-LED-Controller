# ProjectorMaster Architecture v2.0

## 1. System Overview
Firmware for a high-power (1000W) LED projector controller based on the **Adafruit Feather M4 Express (SAMD51)**. It manages a **Mean Well UHP-1500-48** power supply via CAN Bus, controls active liquid cooling, and provides a user interface via OLED and Rotary Encoder.

## 2. Core Architectural Pattern
The system follows a **Dependency Injection (DI)** pattern to decouple logic from hardware.

### 2.1 Hardware Layer (`src/core/Hardware.cpp`)
*   **Role:** The "Composition Root". Instantiates all physical drivers (Tachometers, CAN backend, OLED, Thermistors) and pin configurations.
*   **Key Components:**
    *   `NativeCanBackend`: Adapts the SAMD51 hardware CAN controller to the `ICanBackend` interface.
    *   `TachometerManager`: Handles PWM output and tachometer pulse counting.

### 2.2 AppContext (`include/core/AppContext.h`)
*   **Role:** The "Toolbelt". A struct containing references to all hardware instances.
*   **Purpose:** Passed to the `SystemController` and Services, allowing them to access hardware without relying on global variables.

### 2.3 System Controller (`src/state/SystemController.cpp`)
*   **Role:** The "Brain". Manages the high-level Finite State Machine (FSM) and owns the domain services.
*   **States:**
    1.  **INIT**: Validates sensors, spins up pumps/fans, and checks for hardware faults before enabling high power.
    2.  **RUN**: The main control loop. Updates services, inputs, and UI.
    3.  **ERROR_KILL**: A latched safety state entered upon critical faults (Over-temp, CAN timeout). Disables PSU immediately.

## 3. Domain Services
Logic is divided into isolated services, injected with their specific dependencies.

| Service | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **`PsuService`** | Manages LED current setpoint, slew limiting, and PSU enable/disable logic. | `CanBusManager` |
| **`CoolingService`** | Reads temperatures and sets fan/pump speeds based on configured curves. | `TachometerManager`, `ThermistorManager` |
| **`InputService`** | Processes encoder rotation and power button (arm/disarm) logic. | `EncoderManager`, `PowerButtonManager` |
| **`UiController`** | Renders the system state to the OLED display (Telemetry, Gauges, Errors). | `OledManager` |

## 4. Configuration
All system tuning parameters are centralized in `include/config/`:

*   **`PinMap.h`**: Hardware pin definitions (Native CAN on SDA/SCL, etc.).
*   **`ThermalConfig.h`**: Thermistor beta values, Fan/Pump RPM limits, and **Fan Curves** (Temperature -> PWM mapping).
*   **`PowerConfig.h`**: PSU voltage/current limits, CAN bus bitrate, and Slew rates.

## 5. Safety & Logging
*   **FaultManager**: Monitors telemetry against limits (e.g., LED Temp > 75°C) and triggers state transitions.
*   **ErrorLogger**: Persists fault events to the onboard QSPI Flash (`error_log.csv`) using a FAT filesystem, ensuring crash data is saved even on power loss. The logger captures the exact **Boot Step** if a failure occurs during initialization.

## 6. Protocols
*   **CAN Bus**: Uses 29-bit Extended IDs.
    *   **Current Control**: Uses PMBus Linear11 encoding. 100% LED Power (22A) is scaled relative to the PSU Max (31.3A) to ensure correct current command.
