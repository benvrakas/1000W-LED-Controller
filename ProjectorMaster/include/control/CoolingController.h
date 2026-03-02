#pragma once

#include "systemstate/SystemController.h"

// CoolingController
// ------------------
// Mid-level policy layer for all active cooling devices (radiator fans,
// PSU fan, pump, and aux/lens fan). This class is responsible for deciding
// *what* speeds the cooling channels should run at, based on system
// telemetry and error conditions, while the low-level TachometerManager
// handles *how* those speeds are driven in hardware.

class CoolingController {
public:
    // Initialize any internal state. Called once when entering RUN.
    void begin();

    // Periodic update invoked from the RUN state. The SystemController
    // provides global telemetry and access to state data; cooling policy
    // can be refined here without touching low-level drivers.
    void update(SystemController &sys, unsigned long now);
};

