#include "logging/FaultManager.h"

FaultManager &FaultManager::instance() {
    static FaultManager mgr;
    return mgr;
}

FaultManager::FaultManager()
    : _activeFault(FaultCode::NONE) {}

void FaultManager::raiseFault(FaultCode code) {
    // For now, track a single active fault. This can be expanded to a
    // bitmask or list if multiple simultaneous faults are required.
    _activeFault = code;
}

void FaultManager::clearFault(FaultCode code) {
    if (_activeFault == code) {
        _activeFault = FaultCode::NONE;
    }
}

bool FaultManager::hasActiveFaults() const {
    return _activeFault != FaultCode::NONE;
}

FaultCode FaultManager::getActiveFault() const {
    return _activeFault;
}

void FaultManager::update(SystemController &sys, unsigned long now) {
    (void)sys;
    (void)now;
    // TODO: Add any periodic fault-handling behavior here, such as
    // escalation, auto-clear-after-time, or interaction with the
    // ERROR_KILL state transition policy.
}

