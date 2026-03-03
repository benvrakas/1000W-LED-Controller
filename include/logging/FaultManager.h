#pragma once

#include "state/SystemController.h"

// FaultCode
// ---------
// Enumerates high-level fault conditions the system can experience. This
// list can be extended as new fault conditions are implemented.

enum class FaultCode : uint8_t {
    NONE = 0,

    // PSU / power-path related faults
    CAN_TIMEOUT,        // Lost CAN communication with PSU
    PSU_FAULT,          // PSU reported an internal fault status

    // Thermal and cooling faults
    OVER_TEMP_LED,      // LED junction / MCPCB exceeded safe temperature
    OVER_TEMP_WATER,    // Coolant temperature exceeded safe limit
    COOLING_FAILURE,    // Fan or pump RPM too low for commanded duty

    // UI / input faults
    ENCODER_FAULT,
    BUTTON_STUCK,
};

// FaultManager
// ------------
// Central registry for active faults. Provides a single place where
// subsystems raise or clear faults, and where the RUN and ERROR_KILL
// states query overall system health.

class FaultManager {
 public:
    static FaultManager &instance();

    void raiseFault(FaultCode code);
    void clearFault(FaultCode code);

    bool      hasActiveFaults() const;
    FaultCode getActiveFault() const;  // Expose current fault for logging/telemetry

    // Optional periodic work; can be used to implement time-based fault
    // transitions or latching behavior.
    void update(SystemController &sys, unsigned long now);

 private:
    FaultManager();

    FaultCode _activeFault;
};

