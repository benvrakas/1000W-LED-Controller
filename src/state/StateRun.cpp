#include "state/StateRun.h"

#include "services/CoolingService.h"
#include "services/PsuService.h"
#include "services/InputService.h"
#include "ui/UiController.h"
#include "logging/FaultManager.h"
#include "logging/ErrorLogger.h"

// Handler Function Implementation

// Class Construction
SystemRunning::SystemRunning() 
{}

// Functions Definitions
// Initialization
void SystemRunning::begin() {
    // Placeholder: RUN-state specific initialization, if needed.
}

// Top-level state handler for NORMAL operation
void handleRunState(SystemController &sys, unsigned long now) {
    // Initialize services once when first entering RUN
    static bool initialized = false;

    if (!initialized) {
        sys.cooling.begin();
        sys.psu.begin();
        sys.input.begin();
        sys.ui.begin();
        ErrorLogger::instance().begin();
        initialized = true;
    }

    // Update input semantics (button + encoder)
    sys.input.update(now);
    bool armed = sys.input.isArmed();

    // Handle ON/OFF transitions for PSU control via PsuController
    if (sys.input.edgeArmedOn()) {
        sys.input.forceKnobToZero(); // Ensure knob is reset when arming
        sys.psu.requestOn();
    }

    if (sys.input.edgeArmedOff()) {
        // Force UI setpoint toward 0 via InputService; the slewing logic
        // will then ramp applied current down at up to 100%/s.
        sys.input.forceKnobToZero();
        sys.psu.requestOff();
    }

    // 1. Pass the knob fraction from InputService to PsuController, then update
    sys.psu.setUiSetpointFraction(sys.input.getKnobFraction());
    sys.psu.update(now);

    // 2. Update cooling policy (fans, pump, aux) based on temps + power
    sys.cooling.update(now);

    // Copy cooling state to SystemController globals for UI/logging compatibility
    const CoolingState& cs = sys.cooling.getState();
    sys.globalLedTemp     = cs.ledTempC;
    sys.globalPumpTemp    = cs.waterTempC;
    sys.globalMainFansRPM = cs.mainFanRPM;
    sys.globalAuxFanRPM   = cs.auxFanRPM;
    sys.globalPSUFanRPM   = cs.psuFanRPM;
    sys.globalPumpRPM     = cs.pumpRPM;

    // 3. Build SystemViewModel from services for UI/logging
    SystemViewModel vm;
    vm.psuVoltage       = sys.psu.getVoltage();
    vm.psuCurrent       = sys.psu.getCurrent();
    vm.psuPower         = sys.psu.getPower();
    vm.ledTempC         = cs.ledTempC;
    vm.waterTempC       = cs.waterTempC;
    vm.mainFanRPM       = cs.mainFanRPM;
    vm.auxFanRPM        = cs.auxFanRPM;
    vm.psuFanRPM        = cs.psuFanRPM;
    vm.pumpRPM          = cs.pumpRPM;
    vm.knobFraction     = sys.input.getKnobFraction();
    vm.appliedFraction  = sys.psu.getAppliedCurrentFraction();
    vm.isArmed          = armed;

    // 4. Update fault model and log any new events
    FaultManager::instance().update(sys, now);
    ErrorLogger::instance().update(sys, vm, now);

    // 5. Update the power button LED to reflect armed/ON state
    sys.input.setButtonLed(armed);

    // 6. Finally, render the current UI frame
    sys.ui.update(vm, now);
}
