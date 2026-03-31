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
    : _activeFault(FaultCode::NONE), 
      _ledOvertempStartMs(0), 
      _waterOvertempStartMs(0) {}

void FaultManager::raiseFault(FaultCode code) {
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
    // Skip fault checking if already in ERROR_KILL state
    if (sys.currentState == SystemState::ERROR_KILL) {
        return;
    }

    // 1. Check for overtemperature conditions with temporal filtering (EMF protection)
    float ledTemp = ledThermistor.getCelsius();
    float waterTemp = pumpThermistor.getCelsius();

    // LED Overtemp Debounce - Must persist for 3 seconds to trigger
    if (ledTemp > ThermistorConfig::MAX_TEMP_LED) {
        if (_ledOvertempStartMs == 0) _ledOvertempStartMs = now;
        if (now - _ledOvertempStartMs >= 3000UL) {
            raiseFault(FaultCode::OVER_TEMP_LED);
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
        }
    } else {
        _ledOvertempStartMs = 0;
    }

    // Water Overtemp Debounce - Must persist for 3 seconds
    if (waterTemp > ThermistorConfig::MAX_TEMP_PUMP) {
        if (_waterOvertempStartMs == 0) _waterOvertempStartMs = now;
        if (now - _waterOvertempStartMs >= 3000UL) {
            raiseFault(FaultCode::OVER_TEMP_WATER);
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
        }
    } else {
        _waterOvertempStartMs = 0;
    }

    // 2. Check for PSU CAN communication timeout
    // Trigger if we've seen telemetry before and it stopped.
    // This allows booting on USB without a PSU connected.
    if (psu.hasFault() && psu.telemetryValid()) {
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
}

