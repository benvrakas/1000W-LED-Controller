#include "state/SystemController.h"
#include <Arduino.h>

// SystemController Class Construction
SystemController::SystemController()
    : currentState(SystemState::INIT), globalLedTemp(0.0f), globalPumpTemp(0.0f),
      globalMainFansRPM(0), globalAuxFanRPM(0), globalPSUFanRPM(0), globalPumpRPM(0)
{}

// 1. Set initial machine state cleanly
void SystemController::begin() {
    // Clear the union memory to start clean
    memset(&stateData, 0, sizeof(stateData));

}

// 2. The Main State Machine Loop
// This function runs every cycle of the main loop()
void SystemController::update() {
    switch (currentState) {
        case SystemState::INIT:
            handleInitState(*this, millis()); // Pass this specific instance and global time to the handler
            break;

        case SystemState::RUN:
            handleRunState(*this, millis());
            break;

        case SystemState::ERROR_KILL:
            handleErrorKillState(*this, millis());
            break;
    }
}

// 3. The Transition Logic
void SystemController::transitionTo(SystemState newState) {
    Serial.print(F("State Transition: "));
    
    // Perform any "Cleanup" before switching
    // Example: If leaving RUN, ensure the PSU is told to stop output
    if (currentState == SystemState::RUN && newState == SystemState::ERROR_KILL) {
        // psu.shutdown(); // Safety first
    }

    // Update the state
    currentState = newState;

    // IMPORTANT: Clear the Union!
    // This wipes the memory of the old state so the new state starts fresh.
    memset(&stateData, 0, sizeof(stateData));
}
