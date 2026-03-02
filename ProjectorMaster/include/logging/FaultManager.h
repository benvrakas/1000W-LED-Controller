#pragma once

#include "SystemController.h"

// FaultCode
// ---------
// Enumerates high-level fault conditions the system can experience. This
// list can be extended as new fault conditions are implemented.

enum class FaultCode : uint8_t {
    NONE = 0,
    // TODO: Add concrete fault codes such as OVER_TEMP_LED, OVER_TEMP_WATER,
    // CAN_TIMEOUT, PSU_FAULT, UI_FAULT, etc.
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

    bool hasActiveFaults() const;

    // Optional periodic work; can be used to implement time-based fault
    // transitions or latching behavior.
    void update(SystemController &sys, unsigned long now);

private:
    FaultManager();

    FaultCode _activeFault;
};

