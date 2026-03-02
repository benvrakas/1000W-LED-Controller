#include "services/PsuService.h"

#include <Arduino.h>
#include "drivers/CanBus.h"
#include "util/BoardPins.h"

// Global PSU CAN manager instance from main.cpp
extern CanBusManager psu;

PsuService::PsuService()
    : _uiSetpointFrac(0.0f),
      _appliedCurrentFrac(0.0f),
      _lastUpdateMs(0),
      _slewRatePctPerSec(SLEW_RATE_NORMAL_PCT_PER_SEC),
      _isOn(false) {}

void PsuService::begin() {
    // Initialize timing baseline for slew calculations
    _lastUpdateMs = millis();

    // Ensure we start from 0 current command
    _uiSetpointFrac     = 0.0f;
    _appliedCurrentFrac = 0.0f;
    _slewRatePctPerSec  = SLEW_RATE_NORMAL_PCT_PER_SEC;
    _isOn               = false;

    // Ensure remote gate is in known OFF state
    pinMode(BoardPins::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_REMOTE, LOW);
}

void PsuService::requestOn() {
    if (_isOn) return;
    
    _isOn = true;
    _slewRatePctPerSec = SLEW_RATE_NORMAL_PCT_PER_SEC;
    
    // Enable CAN operation and remote gate
    psu.setOperation(true);
    pinMode(BoardPins::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_REMOTE, HIGH);
}

void PsuService::requestOff() {
    if (!_isOn) return;
    
    _isOn = false;
    _slewRatePctPerSec = SLEW_RATE_SHUTDOWN_PCT_PER_SEC;
    
    // Note: actual CAN disable and remote gate LOW happens in update()
    // once applied current reaches ~0, for safe shutdown sequencing.
}

float PsuService::applySlew(float currentFrac, float targetFrac, float dtSec) const {
    if (dtSec <= 0.0f) {
        return currentFrac;
    }

    // Convert percent-per-second rate to fraction-per-second
    float maxDeltaFracPerSec = _slewRatePctPerSec / 100.0f;
    float maxStep            = maxDeltaFracPerSec * dtSec;

    float error = targetFrac - currentFrac;

    if (error > maxStep) {
        error = maxStep;
    } else if (error < -maxStep) {
        error = -maxStep;
    }

    float next = currentFrac + error;
    if (next < 0.0f) next = 0.0f;
    if (next > 1.0f) next = 1.0f;
    return next;
}

void PsuService::update(unsigned long now) {
    // _uiSetpointFrac is now set externally via setUiSetpointFraction()
    // (typically from InputService before calling update).

    // 1) Apply configurable slew on applied current fraction
    uint32_t dtMs = now - _lastUpdateMs;
    float    dtSec = static_cast<float>(dtMs) / 1000.0f;

    _appliedCurrentFrac = applySlew(_appliedCurrentFrac, _uiSetpointFrac, dtSec);
    _lastUpdateMs       = now;

    // 2) Convert to current and issue CAN command
    float targetCurrentA = _appliedCurrentFrac * MAX_LED_CURRENT_A;

    // For now, approximate: map current fraction directly to a 0–100% level
    // and use CanBusManager's requestPowerPercent API as the "current" set.
    float levelPercent = _appliedCurrentFrac * 100.0f;
    if (levelPercent < 0.0f) levelPercent = 0.0f;
    if (levelPercent > 100.0f) levelPercent = 100.0f;

    (void)targetCurrentA; // Reserved for future use when encoding actual amps

    psu.requestPowerPercent(levelPercent);

    // 3) Allow CAN manager to process telemetry and watchdog
    psu.update(now);

    // 4) If we're in shutdown mode and current has ramped down, disable PSU
    if (!_isOn && _appliedCurrentFrac <= 0.01f) {
        psu.setOperation(false);
        digitalWrite(BoardPins::PIN_PSU_REMOTE, LOW);
    }
}

// Telemetry getters - delegate to CAN manager
float PsuService::getVoltage() const {
    return psu.getTelemetry().voltage;
}

float PsuService::getCurrent() const {
    return psu.getTelemetry().current;
}

float PsuService::getPower() const {
    CanTelemetry t = psu.getTelemetry();
    return t.voltage * t.current;
}

