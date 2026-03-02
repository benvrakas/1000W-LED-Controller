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
    // Lazily construct the domain services the first time we enter RUN.
    static bool initialized = false;
    static CoolingService    coolingService;
    static PsuService        psuService;
    static InputService      inputService;
    static UiController      uiController;

    if (!initialized) {
        coolingService.begin();
        psuService.begin();
        inputService.begin();
        uiController.begin();
        ErrorLogger::instance().begin();
        initialized = true;
    }

    // Update input semantics (button + encoder)
    inputService.update(now);
    bool armed = inputService.isArmed();

    // Handle ON/OFF transitions for PSU control via PsuController
    if (inputService.edgeArmedOn()) {
        psuService.requestOn();
    }

    if (inputService.edgeArmedOff()) {
        // Force UI setpoint toward 0 via InputService; the slewing logic
        // will then ramp applied current down at up to 100%/s.
        inputService.forceKnobToZero();
        psuService.requestOff();
    }

    // 1. Pass the knob fraction from InputService to PsuController, then update
    psuService.setUiSetpointFraction(inputService.getKnobFraction());
    psuService.update(now);

    // 2. Update cooling policy (fans, pump, aux) based on temps + power
    coolingService.update(now);

    // Copy cooling state to SystemController globals for UI/logging compatibility
    const CoolingState& cs = coolingService.getState();
    sys.globalLedTemp     = cs.ledTempC;
    sys.globalPumpTemp    = cs.waterTempC;
    sys.globalMainFansRPM = cs.mainFanRPM;
    sys.globalAuxFanRPM   = cs.auxFanRPM;
    sys.globalPSUFanRPM   = cs.psuFanRPM;
    sys.globalPumpRPM     = cs.pumpRPM;

    // 3. Build SystemViewModel from services for UI/logging
    SystemViewModel vm;
    vm.psuVoltage       = psuService.getVoltage();
    vm.psuCurrent       = psuService.getCurrent();
    vm.psuPower         = psuService.getPower();
    vm.ledTempC         = cs.ledTempC;
    vm.waterTempC       = cs.waterTempC;
    vm.mainFanRPM       = cs.mainFanRPM;
    vm.auxFanRPM        = cs.auxFanRPM;
    vm.psuFanRPM        = cs.psuFanRPM;
    vm.pumpRPM          = cs.pumpRPM;
    vm.knobFraction     = inputService.getKnobFraction();
    vm.appliedFraction  = psuService.getAppliedCurrentFraction();
    vm.isArmed          = armed;

    // 4. Update fault model and log any new events
    FaultManager::instance().update(sys, now);
    ErrorLogger::instance().update(sys.currentState, vm, now);

    // 5. Update the power button LED to reflect armed/ON state
    inputService.setButtonLed(armed);

    // 6. Finally, render the current UI frame
    uiController.update(vm, now);
}
