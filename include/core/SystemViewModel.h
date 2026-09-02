#pragma once

#include <stdint.h>

// SystemViewModel
// ----------------
// Read-only snapshot of system state for UI rendering and error logging.
// Built by the state handler from service outputs, ensuring consumers
// have no direct access to drivers or globals.
//
// This struct acts as the "contract" between the state machine (producer)
// and UI/logging layers (consumers).

struct SystemViewModel {
    // PSU telemetry
    float psuVoltage;
    float psuCurrent;
    float psuPower;

    // Temperatures
    float ledTempC;
    float waterTempC;

    // RPMs
    uint16_t mainFanRPM;
    uint16_t auxFanRPM;
    uint16_t psuFanRPM;
    uint16_t pumpRPM;

    // Input/control state
    float knobFraction;      // 0..1 user setpoint from encoder
    float appliedFraction;   // 0..1 actual LED power (slew-limited)
    bool  isArmed;              // power button armed state
    uint8_t ignoredChannelCount; // channels currently ignored via ERROR_KILL 3s hold (0 = normal)
};
