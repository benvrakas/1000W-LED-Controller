#include "state/SystemController.h"
#include <Arduino.h>

// SystemController Class Construction
SystemController::SystemController(AppContext& ctx)
    : context(ctx),
      cooling(ctx.mainFan, ctx.psuFan, ctx.pump, ctx.auxFan, ctx.ledThermistor, ctx.pumpThermistor),
      psu(ctx.psu),
      input(ctx.encoder, ctx.powerButton),
      ui(ctx.oled),
      currentState(SystemState::INIT), 
      globalLedTemp(0.0f), globalPumpTemp(0.0f),
      globalMainFansRPM(0), globalAuxFanRPM(0), globalPSUFanRPM(0), globalPumpRPM(0)
{}

// 1. Set initial machine state cleanly
void SystemController::begin() {
    // Initialize state data structures
    initData = {};
    runData = {};
    errorData = {};
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
    // If leaving RUN, ensure the PSU is told to stop output
    if (currentState == SystemState::RUN && newState == SystemState::ERROR_KILL) {
        // Hard disable via driver for safety
        context.psu.setOperation(false);
    }

    // Update the state
    currentState = newState;

    // Initialize the data for the new state
    switch (newState) {
        case SystemState::INIT:       initData = {}; break;
        case SystemState::RUN:        runData = {}; break;
        case SystemState::ERROR_KILL: errorData = {}; break;
    }
}
