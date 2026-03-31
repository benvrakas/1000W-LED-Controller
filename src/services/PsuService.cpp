#include "services/PsuService.h"

#include <Arduino.h>
#include "drivers/CanBus.h"
#include "config/PinMap.h"
#include "config/PowerConfig.h"

PsuService::PsuService(CanBusManager& psu)
    : _psu(psu),
      _uiSetpointFrac(0.0f),
      _appliedCurrentFrac(0.0f),
      _lastUpdateMs(0),
      _slewRatePctPerSec(SLEW_RATE_NORMAL_PCT_PER_SEC),
      _isOn(false),
      _shutdownStartTimeMs(0) {}

void PsuService::begin() {
    _lastUpdateMs = millis();
    _uiSetpointFrac     = 0.0f;
    _appliedCurrentFrac = 0.0f;
    _slewRatePctPerSec  = SLEW_RATE_NORMAL_PCT_PER_SEC;
    _isOn               = false;
    _shutdownStartTimeMs = 0;

    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
}

bool PsuService::telemetryValid() const {
    return _psu.telemetryValid();
}

void PsuService::requestOn() {
    if (_isOn) return;
    _isOn = true;
    _slewRatePctPerSec = SLEW_RATE_NORMAL_PCT_PER_SEC;
    _shutdownStartTimeMs = millis(); // Reuse this variable as "StartupStartTime" for grace period
}

void PsuService::requestOff() {
    if (!_isOn) return;
    _isOn = false;
    _slewRatePctPerSec = SLEW_RATE_SHUTDOWN_PCT_PER_SEC;
    _shutdownStartTimeMs = millis();
}

float PsuService::applySlew(float currentFrac, float targetFrac, float dtSec) const {
    if (dtSec <= 0.0f) return currentFrac;
    float maxDeltaFracPerSec = _slewRatePctPerSec / 100.0f;
    float maxStep            = maxDeltaFracPerSec * dtSec;
    float error = targetFrac - currentFrac;
    if (error > maxStep) error = maxStep;
    else if (error < -maxStep) error = -maxStep;
    float next = currentFrac + error;
    if (next < 0.0f) next = 0.0f;
    if (next > 1.0f) next = 1.0f;
    return next;
}

void PsuService::update(unsigned long now) {
    uint32_t dtMs = now - _lastUpdateMs;
    float    dtSec = static_cast<float>(dtMs) / 1000.0f;

    _appliedCurrentFrac = applySlew(_appliedCurrentFrac, _uiSetpointFrac, dtSec);
    _lastUpdateMs       = now;

    float targetCurrentA = _appliedCurrentFrac * MAX_LED_CURRENT_A;
    float levelPercent = (targetCurrentA / CanBusConfig::MAX_CURRENT_A) * 100.0f;
    
    if (levelPercent < 0.0f) levelPercent = 0.0f;
    if (levelPercent > 100.0f) levelPercent = 100.0f;

    // 1. Set current parameters first
    _psu.requestPowerPercent(levelPercent);

    // 2. Process CAN logic
    _psu.update(now);

    // 3. Operational Logic & Safety
    if (_isOn) {
        // Safety: only raise physical enable pin if CAN is responding
        // AND we don't have a sticky fault.
        // GRACE PERIOD: Allow A4 to stay HIGH for 5 seconds during startup 
        // even without telemetry to "wake up" the PSU CAN controller.
        bool gracePeriod = (now - _shutdownStartTimeMs < 5000) && (_shutdownStartTimeMs != 0);
        
        if ((_psu.telemetryValid() || gracePeriod) && !_psu.hasFault()) {
            _psu.setOperation(true); // Sends 0x80 (ON)
            digitalWrite(PinMap::PIN_PSU_REMOTE, HIGH);
        } else {
            // EMERGENCY STOP: Kill PSU if comms drop during operation
            // (Wait until communication is lost or a fault is detected)
            digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
            _psu.setOperation(false);
        }
    } else {
        // Normal Shutdown sequence
        bool timeoutReached = (now - _shutdownStartTimeMs > 3000);
        if (_appliedCurrentFrac <= 0.01f || timeoutReached) {
            _psu.setOperation(false);
            digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
        }
    }
}

float PsuService::getVoltage() const { return _psu.getTelemetry().voltage; }
float PsuService::getCurrent() const { return _psu.getTelemetry().current; }
float PsuService::getPower() const { 
    CanTelemetry t = _psu.getTelemetry();
    return t.voltage * t.current; 
}
