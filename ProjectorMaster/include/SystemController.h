#pragma once


enum class SystemState {
    INIT,       // System is booting
    ONOFF,    // Normal operation
    ERROR_KILL   // Critical error or hang up occured and system shut down
};

//Data stored by each system state, this is wiped after state change
struct InitData {
    uint8_t bootStep;
    unsigned long lastStepTime;
    bool systemReady;
    bool systemError;
};

struct OnOffData {
    unsigned long lastStepTime;
    bool systemError;
};

struct ErrorKillData {
    unsigned long lastStepTime;
};

//High level controller that changes machine states
struct SystemController {
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
    union {
        InitData init;
        OnOffData onoff;
        ErrorKillData error;
    } stateData; // Variable holds data for current state
};

// Handle function prototypes for SystemController object (sys), see main.cpp and SystemController.cpp 
void handleErrorKillState(SystemController &sys, unsigned long now);
void handleInitState(SystemController &sys, unsigned long now);
void handleOnOffState(SystemController &sys, unsigned long now);