#pragma once

#include "systemstate/SystemController.h"

// PsuController
// -------------
// High-level wrapper around the PSU CAN manager. This controller owns the
// notion of LED power setpoint, PSU operating mode, and interpreted PSU
// telemetry that other subsystems (cooling, UI, logging) can consume.

class PsuController {
public:
    PsuController();

    // Bring the PSU control layer online. Intended to be called once from
    // the RUN state when the system first transitions from INIT.
    void begin();

    // Periodic update from RUN. Responsible for updating cached power
    // setpoints, reading telemetry, and enforcing any PSU-level policy.
    void update(SystemController &sys, unsigned long now);
};

