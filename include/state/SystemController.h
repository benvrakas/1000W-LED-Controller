#pragma once

#include <Arduino.h>
#include "core/AppContext.h"
#include "services/CoolingService.h"
#include "services/PsuService.h"
#include "services/InputService.h"
#include "ui/UiController.h"

enum class SystemState {
    INIT,       // System is booting
    RUN,        // Normal operation
    ERROR_KILL   // Critical error or hang up occured and system shut down
};

//Data stored by each system state, this is wiped after state change
struct InitData {
    uint8_t bootStep;
    unsigned long lastStepTime;
    bool systemReady;
    bool systemError;
};

struct RunData {
    unsigned long lastStepTime;
    bool systemError;
};

struct ErrorKillData {
    unsigned long lastStepTime;
};

//High level controller that changes machine states
struct SystemController {
    SystemController(AppContext& context);

    // Dependencies & Services
    AppContext&    context;
    CoolingService cooling;
    PsuService     psu;
    InputService   input;
    UiController   ui;

    SystemState currentState;
    // Persistantly saved state data, available to all states
    float globalLedTemp;
    float globalPumpTemp;
    uint16_t globalMainFansRPM;
    uint16_t globalAuxFanRPM;
    uint16_t globalPSUFanRPM;
    uint16_t globalPumpRPM;
    //
    //Error data variable

    // API Functions for the state machine
    void begin();
    void update();
    void transitionTo(SystemState newState);

    // Reserve an amount of memory equal to the largest member (machine state) but only store one member at a time
    // Saves data by not reserving memory for every struct all the time
    // Union removed to allow persistent state data for logging and simplified transition logic
    InitData initData;
    RunData runData;
    ErrorKillData errorData;
};

// Handle function prototypes for SystemController object (sys), see main.cpp and SystemController.cpp 
void handleErrorKillState(SystemController &sys, unsigned long now);
void handleInitState(SystemController &sys, unsigned long now);
void handleRunState(SystemController &sys, unsigned long now);
