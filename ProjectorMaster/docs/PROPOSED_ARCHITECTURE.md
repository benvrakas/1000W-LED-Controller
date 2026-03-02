# Proposed Architecture: Dependency Injection & Modular Context

## Critique of Current Design
The current architecture (v2.0) is a significant improvement over the original "spaghetti code", organizing logic into Services and Drivers. However, it relies heavily on **global variables** (`extern TachometerManager pump;`) accessed directly by services (`CoolingService`).

**Drawbacks of Globals:**
1.  **Tight Coupling**: `CoolingService` is hardcoded to talk to specific global variables. You can't easily swap the fan instance for a test mock or a different pin configuration without modifying the service code.
2.  **Hidden Dependencies**: Looking at `CoolingService.h`, you can't tell it needs a Tachometer. You have to read the `.cpp` file to see the `extern` usage.
3.  **Initialization Order**: Relies on C++ global constructor order, which can be unpredictable (though single-file placement mitigates this).

## The "Ideal" Design: Dependency Injection

If designing from scratch, I would use **Dependency Injection (DI)** with a **Context Object**.

### 1. The Context Object
Instead of scattered globals, gather all hardware drivers into a single container struct.

```cpp
struct HardwareContext {
    TachometerManager& mainFan;
    TachometerManager& pump;
    CanBusManager&     psu;
    OledManager&       oled;
    // ... other drivers
};
```

### 2. Service Construction
Services accept their dependencies in their constructor. They store *references* to the drivers they need.

```cpp
class CoolingService {
public:
    // Explicit dependencies!
    CoolingService(TachometerManager& fan, TachometerManager& pump, ThermistorManager& sensor);
    void update(); 

private:
    TachometerManager& _fan;
    TachometerManager& _pump;
    ThermistorManager& _sensor;
};
```

### 3. Application Class (The "Composition Root")
In `main.cpp` (or `App.cpp`), you instantiate the hardware, create the context, and wire up the services.

```cpp
// 1. Create Drivers
TachometerManager realMainFan(PIN_FAN, ...);
TachometerManager realPump(PIN_PUMP, ...);

// 2. Create Services (Injecting Drivers)
CoolingService cooling(realMainFan, realPump, realThermistor);
PsuService     power(realCanBus);

// 3. Create System Controller (Injecting Services)
SystemController sys(cooling, power);

void setup() {
    sys.begin();
}
```

### Benefits
*   **Testability**: You can pass a `MockFan` to `CoolingService` to test the logic without hardware.
*   **Clarity**: The header file tells you exactly what the service needs to work.
*   **Flexibility**: Easy to run two instances of `CoolingService` for dual-loop cooling if needed.

## Alternative: RTOS (Real-Time Operating System)
For a control system like this (Safety critical + UI + Comms), a lightweight RTOS (FreeRTOS, native to ESP32 but available for SAMD51) is often superior.

*   **Task 1 (High Priority):** Control Loop (100Hz). PID, Safety Checks. *Never blocks.*
*   **Task 2 (Medium):** Comms (CAN bus handling).
*   **Task 3 (Low):** UI (OLED updates). *Can block on slow I2C without freezing the fan control.*

The current "Super Loop" architecture (`millis()` checks) simulates this but requires careful non-blocking coding rules.

## Summary
While the current architecture is functional and clean enough for a single-developer firmware project, moving to **Dependency Injection** would make it professional-grade, testable, and robust against future complexity.
