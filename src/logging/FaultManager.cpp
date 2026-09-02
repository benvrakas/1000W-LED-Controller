#include "logging/FaultManager.h"
#include "drivers/Thermistors.h"
#include "drivers/Tachometers.h"
#include "drivers/CanBus.h"
#include "drivers/Encoder.h"
#include "config/PowerConfig.h"
#include <string.h>

IgnorableSource identifyFaultSource(FaultCode active, const char* detail) {
    switch (active) {
        case FaultCode::OVER_TEMP_LED:   return IgnorableSource::LED_TEMP;
        case FaultCode::OVER_TEMP_WATER: return IgnorableSource::WATER_TEMP;
        case FaultCode::CAN_TIMEOUT:
        case FaultCode::PSU_FAULT:       return IgnorableSource::PSU_COMMS;
        case FaultCode::ENCODER_FAULT:   return IgnorableSource::ENCODER;
        case FaultCode::COOLING_FAILURE:
        case FaultCode::INIT_FAILED:
            if (!detail) return IgnorableSource::NONE;
            if (strcmp(detail, "PUMP") == 0)     return IgnorableSource::PUMP;
            if (strcmp(detail, "MAIN FAN") == 0) return IgnorableSource::MAIN_FAN;
            if (strcmp(detail, "PSU FAN") == 0)  return IgnorableSource::PSU_FAN;
            if (strcmp(detail, "AUX FAN") == 0)  return IgnorableSource::AUX_FAN;
            return IgnorableSource::NONE;
        default:
            return IgnorableSource::NONE;
    }
}

// External globals for fault checking
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;
extern TachometerManager pump;
extern TachometerManager mainFan;
extern TachometerManager psuFan;
extern TachometerManager auxFan;
extern CanBusManager psu;
extern EncoderManager encoder;

FaultManager &FaultManager::instance() {
    static FaultManager mgr;
    return mgr;
}

FaultManager::FaultManager()
    : _activeFault(FaultCode::NONE),
      _faultDetail(nullptr),
      _ledOvertempStartMs(0),
      _waterOvertempStartMs(0),
      _pumpStallStartMs(0),
      _mainFanStallStartMs(0),
      _psuFanStallStartMs(0),
      _auxFanStallStartMs(0),
      _encoderFaultWindowStartMs(0),
      _encoderIllegalAccum(0) {}

bool FaultManager::checkStallDebounced(TachometerManager& fan, unsigned long& stallStartMs, unsigned long now) {
    if (fan.getStallStatus()) {
        if (stallStartMs == 0) stallStartMs = now;
        return (now - stallStartMs) >= TachometerConfig::STALL_FAULT_DEBOUNCE_MS;
    }
    stallStartMs = 0;
    return false;
}

void FaultManager::raiseFault(FaultCode code, const char* detail) {
    _activeFault = code;
    _faultDetail = detail;

    // Capture what every monitored channel looked like at this instant, so
    // the ERROR_KILL diagnostics screen can show more than just the one
    // condition that happened to trip first.
    _snapshot.ledTempC   = ledThermistor.getCelsius();
    _snapshot.waterTempC = pumpThermistor.getCelsius();
    _snapshot.pumpRPM    = pump.getRPM();
    _snapshot.pumpDuty   = pump.getDuty();
    _snapshot.mainFanRPM = mainFan.getRPM();
    _snapshot.mainFanDuty = mainFan.getDuty();
    _snapshot.psuFanRPM  = psuFan.getRPM();
    _snapshot.psuFanDuty = psuFan.getDuty();
    _snapshot.auxFanRPM  = auxFan.getRPM();
    _snapshot.auxFanDuty = auxFan.getDuty();
    _snapshot.canFaultFlag      = psu.hasFault();
    _snapshot.canTelemetryValid = psu.telemetryValid();
    _snapshot.encoderIllegalAccum = _encoderIllegalAccum;
    _snapshot.timestampMs = millis();
}

void FaultManager::resetDebounceTimers() {
    _ledOvertempStartMs   = 0;
    _waterOvertempStartMs = 0;
    _pumpStallStartMs     = 0;
    _mainFanStallStartMs  = 0;
    _psuFanStallStartMs   = 0;
    _auxFanStallStartMs   = 0;
    _encoderFaultWindowStartMs = 0;
    _encoderIllegalAccum  = 0;
}

void FaultManager::clearFault(FaultCode code) {
    if (_activeFault == code) {
        _activeFault = FaultCode::NONE;
        _faultDetail = nullptr;
    }
}

bool FaultManager::hasActiveFaults() const {
    return _activeFault != FaultCode::NONE;
}

FaultCode FaultManager::getActiveFault() const {
    return _activeFault;
}

const char* FaultManager::getFaultDetail() const {
    return _faultDetail;
}

const FaultSnapshot& FaultManager::getSnapshot() const {
    return _snapshot;
}

void FaultManager::update(SystemController &sys, unsigned long now) {
    // Skip fault checking if already in ERROR_KILL state
    if (sys.currentState == SystemState::ERROR_KILL) {
        return;
    }

    // 1. Check for overtemperature conditions with temporal filtering (EMF
    // protection). Each check is individually gated by its own per-channel
    // ignore flag (see SystemController.h) -- every fault category is
    // ignorable via the ERROR_KILL 3s hold, not just cooling channels.
    float ledTemp = ledThermistor.getCelsius();
    float waterTemp = pumpThermistor.getCelsius();

    // LED Overtemp Debounce - Must persist for 3 seconds to trigger
    if (!sys.globalLedTempIgnored && ledTemp > ThermistorConfig::MAX_TEMP_LED) {
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
    if (!sys.globalWaterTempIgnored && waterTemp > ThermistorConfig::MAX_TEMP_PUMP) {
        if (_waterOvertempStartMs == 0) _waterOvertempStartMs = now;
        if (now - _waterOvertempStartMs >= 3000UL) {
            raiseFault(FaultCode::OVER_TEMP_WATER);
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
        }
    } else {
        _waterOvertempStartMs = 0;
    }

    // 2. Check for PSU CAN communication timeout (CAN mode only).
    // Trigger if we've seen telemetry before and it stopped.
    // This allows booting on USB without a PSU connected.
    if (!sys.globalPsuCommsIgnored && PsuControlConfig::PSU_CONTROL_VIA_CAN) {
        if (psu.hasFault() && psu.telemetryValid()) {
            raiseFault(FaultCode::CAN_TIMEOUT);
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
        }
    }

    // 3. Check for cooling failures (pump or fan stall). Debounced: a stall
    // reading must persist past STALL_GRACE_MS's boundary for
    // STALL_FAULT_DEBOUNCE_MS before latching, since RPM is only recomputed
    // every RPM_COMPUTE_INTERVAL and a single sample right at the grace
    // boundary can catch a healthy fan still mid-ramp-up.
    if (!sys.globalPumpIgnored && checkStallDebounced(pump, _pumpStallStartMs, now)) {
        raiseFault(FaultCode::COOLING_FAILURE, "PUMP");
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    if (!sys.globalMainFanIgnored && checkStallDebounced(mainFan, _mainFanStallStartMs, now)) {
        raiseFault(FaultCode::COOLING_FAILURE, "MAIN FAN");
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    // PSU fan tach monitoring can also be disabled wholesale at compile
    // time -- see TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED. Skipped
    // if either that or the runtime ignore flag says to skip it.
    if (TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED && !sys.globalPsuFanIgnored &&
        checkStallDebounced(psuFan, _psuFanStallStartMs, now)) {
        raiseFault(FaultCode::COOLING_FAILURE, "PSU FAN");
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    if (!sys.globalAuxFanIgnored && checkStallDebounced(auxFan, _auxFanStallStartMs, now)) {
        raiseFault(FaultCode::COOLING_FAILURE, "AUX FAN");
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }

    // 4. Check for encoder signal integrity. Illegal (double-bit) quadrature
    // transitions are counted in the encoder ISR; a burst within a short
    // window indicates a failing/disconnected channel rather than the
    // occasional stray edge expected now that the hardware glitch filter is
    // disabled on the encoder's EIC lines (see SystemStartup::isrInit).
    uint16_t illegalTransitions = encoder.consumeIllegalTransitionCount();
    if (!sys.globalEncoderIgnored && illegalTransitions > 0) {
        if (now - _encoderFaultWindowStartMs > ENCODER_FAULT_WINDOW_MS) {
            _encoderFaultWindowStartMs = now;
            _encoderIllegalAccum = 0;
        }
        _encoderIllegalAccum += illegalTransitions;

        // Debug visibility for bench-tuning ENCODER_FAULT_THRESHOLD: logs
        // real-world illegal-transition rates without affecting detection.
        Serial.print(F("Encoder: illegal transitions in window: "));
        Serial.print(_encoderIllegalAccum);
        Serial.print(F(" / "));
        Serial.println(ENCODER_FAULT_THRESHOLD);

        if (_encoderIllegalAccum >= ENCODER_FAULT_THRESHOLD) {
            raiseFault(FaultCode::ENCODER_FAULT);
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
        }
    }
}

