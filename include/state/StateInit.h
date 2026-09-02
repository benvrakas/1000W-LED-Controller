#pragma once

#include <Arduino.h>

class SystemController; // Forward declaration

class SystemStartup {
public:
    //Class Construction
    SystemStartup();

    //Getter for checking each step status
    bool getStepStatus(uint8_t bootStep) const;

    //Setter for each step status
    void setStepStatus(uint8_t bootStep, bool status);

    //Initialization and verification functions
    // Init functions must be idempotent: INIT is re-entered from ERROR_KILL
    // via hold-to-clear for a full re-validation pass, so each of these runs
    // again on every pass. They are deliberately NOT one-shot guarded -- see
    // the comment on _spinupStartMs below.
    void boardPinsInit(SystemController& sys);
    void boardPinsVerify(uint8_t bootStep);
    void isrInit();
    void pumpInit(SystemController& sys);
    void pumpVerify(SystemController& sys, uint8_t bootStep, unsigned long currentMillis);
    void fansInit(SystemController& sys);
    void fansVerify(SystemController& sys, uint8_t bootStep, unsigned long currentMillis);
    // Just the .update() calls fansVerify() needs, without the step-status
    // side effect -- lets case 2 (pump) keep the fans' RPM/pulse windows
    // fresh while they spin up concurrently, without misattributing a fan
    // pass/fail to whatever bootStep case 2 happens to be.
    void fansUpdate(SystemController& sys, unsigned long currentMillis);
    void psuInit(SystemController& sys);
    void psuVerify(uint8_t bootStep);
    void displayInit(SystemController& sys);
    void displayVerify(uint8_t bootStep);

    // Logs which cooling channel(s) failed to reach speed for boot steps 2/3.
    // The INIT_FAILED blink code only encodes the step number, so without
    // this a failed cooling boot can't be narrowed down to a channel.
    void reportCoolingStepFailure(SystemController& sys, uint8_t bootStep);

    // Same underlying per-channel check as reportCoolingStepFailure(), but
    // returns the first failing channel's name (or nullptr if none is below
    // stall) for use as INIT_FAILED's fault detail -- so the diagnostics
    // screen can attribute an INIT-time cooling failure to a channel the
    // same way it already does for a runtime COOLING_FAILURE. Skips the PSU
    // fan when its tach monitoring is disabled (see
    // TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED).
    const char* identifyCoolingFailure(SystemController& sys, uint8_t bootStep) const;

private:
    //Systems Checks
    bool _boardPinsReady;
    bool _pumpReady;
    bool _fansReady;
    bool _psuReady;
    bool _displayReady;
    bool _encoderReady;
    bool _thermistorsReady;

    // Timestamp of the first pumpInit()/fansInit() call in THIS boot pass, so
    // the corresponding verify() can hold at spin-up duty for SPINUP_MS
    // before judging the tach. 0 = not started yet.
    //
    // Note there are deliberately no _pumpInitDone/_fansInitDone/_psuInitDone
    // latches here: a SystemStartup now lives in InitData and is wiped by
    // SystemController::transitionTo(), so "already done" state cannot leak
    // across a fault clear. It used to, via a function-local static in
    // handleInitState(), which meant boardPinsInit() re-ran and stole the fan
    // PWM pins back to GPIO while fansInit() was skipped and never restored
    // them.
    unsigned long _pumpSpinupStartMs;
    unsigned long _fansSpinupStartMs;

    //Helper to verify pin 
    bool isPinSetAsOutput(uint8_t pin) const;
    bool isPinSetAsInput(uint8_t pin) const;
};

