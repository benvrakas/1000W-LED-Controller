#pragma once

#include <Arduino.h>
#include "core/AppContext.h"
#include "services/CoolingService.h"
#include "services/PsuService.h"
#include "services/InputService.h"
#include "ui/UiController.h"
#include "state/StateInit.h"

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

    // Per-pass boot progress. Lives here rather than as a function-local
    // static in handleInitState() so transitionTo() wipes it along with the
    // rest of InitData -- INIT is re-entered from ERROR_KILL's hold-to-clear
    // and must genuinely re-run every step, not silently skip the ones a
    // previous pass had already completed.
    SystemStartup startup;
};

struct RunData {
    unsigned long lastStepTime;
    bool systemError;

    // Same reasoning as InitData::startup: one-shot service startup must
    // re-run on every entry into RUN, otherwise a fault clear leaves
    // CoolingService::_lastUpdateMs and the PI integrals stale.
    bool servicesStarted;
};

struct ErrorKillData {
    unsigned long lastStepTime;

    // Diagnostics-screen navigation state (encoder + button), reset to zero
    // on every fresh transition into ERROR_KILL along with the rest of this
    // struct -- see SystemController::transitionTo().
    bool          navBaselineSet;
    int16_t       navBaselineCounts;
    uint8_t       currentPage;
    bool          btnWasPressed;
    unsigned long btnPressStartMs;

    // Long-press-to-ignore (see handleErrorKillState): avoids re-logging the
    // "nothing to ignore" message every tick while the button stays held
    // past 3s on a fault with no ignorable channel (e.g. a board-pins INIT
    // failure).
    bool          ignoreBlockedReported;
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
    uint8_t  globalMainFanDuty;
    uint8_t  globalAuxFanDuty;
    uint8_t  globalPSUFanDuty;
    uint8_t  globalPumpDuty;

    // Per-channel operator-requested ignores, set only via the ERROR_KILL 3s
    // hold (see handleErrorKillState). Each flag disables fault checking for
    // exactly one monitored channel -- the one that was actually latched
    // when the hold fired (see FaultManager::identifyFaultSource) -- so a
    // misbehaving channel can be silenced while everything else stays fully
    // monitored. None of these are ever cleared in software; only a
    // physical power cycle resets them.
    bool globalLedTempIgnored   = false;
    bool globalWaterTempIgnored = false;
    bool globalPumpIgnored      = false;
    bool globalMainFanIgnored   = false;
    bool globalPsuFanIgnored    = false;
    bool globalAuxFanIgnored    = false;
    bool globalPsuCommsIgnored  = false;
    bool globalEncoderIgnored   = false;

    // Count of the above currently set -- drives the NeoPixel warning color
    // and the RUN screen's "[N IGNORED]" banner (0 = normal operation).
    uint8_t ignoredChannelCount() const {
        return globalLedTempIgnored + globalWaterTempIgnored + globalPumpIgnored +
               globalMainFanIgnored + globalPsuFanIgnored + globalAuxFanIgnored +
               globalPsuCommsIgnored + globalEncoderIgnored;
    }
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
