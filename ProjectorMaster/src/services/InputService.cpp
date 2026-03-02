#include "services/InputService.h"
#include "util/BoardPins.h"
#include <Arduino.h>

// Use existing global driver instances for now; these can be
// migrated into an AppContext later without changing this service's
// public API.
extern EncoderManager     encoder;
extern PowerButtonManager powerButton;

InputService::InputService()
    : _armed(false),
      _prevArmed(false),
      _edgeArmedOn(false),
      _edgeArmedOff(false),
      _knobFraction(0.0f) {}

void InputService::begin() {
    // Seed state from current button status and encoder position.
    _armed     = powerButton.isArmed();
    _prevArmed = _armed;

    int16_t counts = encoder.getCounts();
    if (counts < 0) counts = 0;
    if (counts > EncoderManager::MAX_COUNTS_RANGE) {
        counts = EncoderManager::MAX_COUNTS_RANGE;
    }

    _knobFraction = static_cast<float>(counts) /
                    static_cast<float>(EncoderManager::MAX_COUNTS_RANGE);

    _edgeArmedOn  = false;
    _edgeArmedOff = false;
}

void InputService::update(unsigned long now) {
    (void)now;

    // Update button semantics first
    powerButton.update(now);

    bool currentArmed = powerButton.isArmed();

    _edgeArmedOn  = (! _prevArmed) && currentArmed;
    _edgeArmedOff = _prevArmed && (! currentArmed);

    _prevArmed = currentArmed;
    _armed     = currentArmed;

    // Update encoder-derived knob fraction
    int16_t counts = encoder.getCounts();
    if (counts < 0) counts = 0;
    if (counts > EncoderManager::MAX_COUNTS_RANGE) {
        counts = EncoderManager::MAX_COUNTS_RANGE;
    }

    _knobFraction = static_cast<float>(counts) /
                    static_cast<float>(EncoderManager::MAX_COUNTS_RANGE);
}

void InputService::forceKnobToZero() {
    encoder.setCounts(0);
    _knobFraction = 0.0f;
}

void InputService::setButtonLed(bool on) {
    digitalWrite(BoardPins::PIN_SW_LED, on ? HIGH : LOW);
}

