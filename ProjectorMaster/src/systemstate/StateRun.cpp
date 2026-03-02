#include "systemstate/StateRun.h"

#include "control/CoolingController.h"
#include "control/PsuController.h"
#include "ui/UiController.h"
#include "logging/FaultManager.h"
#include "logging/ErrorLogger.h"

//Handler Function Implementation

//Class Construction
SystemRunning::SystemRunning() 
{}

//Functions Definitions
//Initialization
void SystemRunning::begin() {
    // Placeholder: RUN-state specific initialization, if needed.
}

// Top-level state handler for NORMAL operation
void handleRunState(SystemController &sys, unsigned long now) {
    // Lazily construct the domain controllers the first time we enter RUN.
    static bool initialized = false;
    static CoolingController coolingController;
    static PsuController     psuController;
    static UiController      uiController;

    if (!initialized) {
        coolingController.begin();
        psuController.begin();
        uiController.begin();
        ErrorLogger::instance().begin();
        initialized = true;
    }

    // 1. Update PSU control/telemetry
    psuController.update(sys, now);

    // 2. Update cooling policy (fans, pump, aux) based on temps + power
    coolingController.update(sys, now);

    // 3. Update fault model and log any new events
    FaultManager::instance().update(sys, now);
    ErrorLogger::instance().update(sys, now);

    // 4. Finally, render the current UI frame
    uiController.update(sys, now);
}


