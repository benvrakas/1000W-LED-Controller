#pragma once

#include <Arduino.h>

// CoolingState
// ------------
// Read-only snapshot of current cooling system telemetry, returned by
// CoolingService::getState() for use by UI and logging systems.

struct CoolingState {
    float    ledTempC;
    float    waterTempC;
    uint16_t mainFanRPM;
    uint16_t auxFanRPM;
    uint16_t psuFanRPM;
    uint16_t pumpRPM;
};

// Forward declaration
struct SystemController;

// CoolingService
// ---------------
// Mid-level policy layer for all active cooling devices (radiator fans,
// PSU fan, pump, and aux/lens fan). This class is responsible for deciding
// *what* speeds the cooling channels should run at, based on system
// telemetry and error conditions, while the low-level TachometerManager
// handles *how* those speeds are driven in hardware.

class CoolingService {
public:
    CoolingService();

    // Initialize any internal state. Called once when entering RUN.
    void begin();

    // Periodic update invoked from the RUN state. Reads temperatures,
    // applies cooling policy, and drives PWM outputs.
    void update(unsigned long now);

    // Get the latest cooling state (temperatures + RPMs)
    const CoolingState& getState() const { return _state; }

private:
    CoolingState _state;
};

