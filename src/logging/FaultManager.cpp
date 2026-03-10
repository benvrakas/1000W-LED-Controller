#include "logging/FaultManager.h"
#include "drivers/Thermistors.h"
#include "drivers/Tachometers.h"
#include "drivers/CanBus.h"

// External globals for fault checking
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;
extern TachometerManager pump;
extern TachometerManager mainFan;
extern TachometerManager psuFan;
extern TachometerManager auxFan;
extern CanBusManager psu;

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
    (void)now;

    // Skip fault checking if already in ERROR_KILL state
    if (sys.currentState == SystemState::ERROR_KILL) {
        return;
    }

    // 1. Check for overtemperature conditions
    float ledTemp = ledThermistor.getCelsius();
    float waterTemp = pumpThermistor.getCelsius();

    if (ledTemp > ThermistorConfig::MAX_TEMP_LED) {
        raiseFault(FaultCode::OVER_TEMP_LED);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    if (waterTemp > ThermistorConfig::MAX_TEMP_PUMP) {
        raiseFault(FaultCode::OVER_TEMP_WATER);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    // 2. Check for PSU CAN communication timeout
    if (psu.hasFault()) {
        raiseFault(FaultCode::CAN_TIMEOUT);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    // 3. Check for cooling failures (pump or fan stall)
    if (pump.getStallStatus()) {
        raiseFault(FaultCode::COOLING_FAILURE);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    if (mainFan.getStallStatus()) {
        raiseFault(FaultCode::COOLING_FAILURE);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    if (psuFan.getStallStatus()) {
        raiseFault(FaultCode::COOLING_FAILURE);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    if (auxFan.getStallStatus()) {
        raiseFault(FaultCode::COOLING_FAILURE);
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    // If we reach here and had a fault before, clear it
    // (This allows recovery from transient faults if needed)
}

