#include "drivers/Tachometers.h"

// ---------------------------------------------------------------------------
// TachometerManager - PWM output + tachometer RPM feedback (fan curve driven)
// ---------------------------------------------------------------------------

TachometerManager::TachometerManager(uint8_t pwmPin, uint8_t tachPin,
                                     uint8_t minDeadStart, uint16_t maxRPM, uint16_t stallRPM)
    : _pwmPin(pwmPin),
      _tachPin(tachPin),
      _minDeadStart(minDeadStart),
      _maxRPM(maxRPM),
      _stallRPM(stallRPM),
      _currentDuty(0),
      _pulseCount(0),
      _currentRPM(0),
      _lastRPMCompute(0)
{}

void TachometerManager::begin() {
    pinMode(_pwmPin, OUTPUT);

    // Tach signals are push-pull CMOS from ADuM isolators,
    // already referenced to the clean 3.3V domain. No pull-up required.
    pinMode(_tachPin, INPUT);

    analogWriteResolution(8);
    analogWrite(_pwmPin, 0);
}

void TachometerManager::setDuty(uint8_t duty) {
    // Apply dead-start minimum if duty is non-zero but below threshold
    if (duty > 0 && duty < _minDeadStart) {
        duty = _minDeadStart;
    }
    _currentDuty = duty;
    analogWrite(_pwmPin, _currentDuty);
}

uint16_t TachometerManager::getRPM() const {
    return _currentRPM;
}

uint8_t TachometerManager::getDuty() const {
    return _currentDuty;
}

uint32_t TachometerManager::getPulseCount() const {
    return _pulseCount;
}

bool TachometerManager::getStallStatus() const {
    return (_currentDuty > 0 && _currentRPM <= _stallRPM);
}

void TachometerManager::stop() {
    _currentDuty = 0;
    analogWrite(_pwmPin, 0);
}

void TachometerManager::stopSlow() {
    // Gradually reduce duty cycle - call repeatedly from main loop
    if (_currentDuty > 0) {
        _currentDuty = (_currentDuty > 10) ? (_currentDuty - 10) : 0;
        analogWrite(_pwmPin, _currentDuty);
    }
}

void TachometerManager::handleTachoInterrupt() {
    _pulseCount++;
}

void TachometerManager::update(unsigned long currentMillis) {
    calculateRPM(currentMillis);
}

void TachometerManager::calculateRPM(unsigned long currentMillis) {
    unsigned long duration = currentMillis - _lastRPMCompute;

    if (duration >= TachometerConfig::RPM_COMPUTE_INTERVAL) {
        // Atomically read and clear pulse count
        noInterrupts();
        uint32_t capturedPulses = _pulseCount;
        _pulseCount = 0;
        interrupts();

        // RPM = (pulses per period) * (60000 ms/min) / (period in ms) / 2 (pulses per rev)
        // Simplified: RPM = capturedPulses * 30000 / duration
        _currentRPM = static_cast<uint16_t>((capturedPulses * 30000UL) / duration);
        
        // Clamp to max RPM
        if (_currentRPM > _maxRPM) {
            _currentRPM = _maxRPM;
        }

        _lastRPMCompute = currentMillis;
    }
}

// ISR Bridge Functions are defined in src/util/HardwareBridges.cpp
