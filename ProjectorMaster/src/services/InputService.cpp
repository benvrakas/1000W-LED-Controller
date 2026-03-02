#include "services/InputService.h"
#include "util/BoardPins.h"
#include <Arduino.h>

InputService::InputService(EncoderManager& encoder, PowerButtonManager& powerButton)
    : _encoder(encoder), _powerButton(powerButton),
      _armed(false),
      _prevArmed(false),
      _edgeArmedOn(false),
      _edgeArmedOff(false),
      _knobFraction(0.0f) {}

void InputService::begin() {
    // Seed state from current button status and encoder position.
    _armed     = _powerButton.isArmed();
    _prevArmed = _armed;

    int16_t counts = _encoder.getCounts();
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
    _powerButton.update(now);

    bool currentArmed = _powerButton.isArmed();

    _edgeArmedOn  = (! _prevArmed) && currentArmed;
    _edgeArmedOff = _prevArmed && (! currentArmed);

    _prevArmed = currentArmed;
    _armed     = currentArmed;

    // Update encoder-derived knob fraction
    int16_t counts = _encoder.getCounts();
    if (counts < 0) counts = 0;
    if (counts > EncoderManager::MAX_COUNTS_RANGE) {
        counts = EncoderManager::MAX_COUNTS_RANGE;
    }

    _knobFraction = static_cast<float>(counts) /
                    static_cast<float>(EncoderManager::MAX_COUNTS_RANGE);
}

void InputService::forceKnobToZero() {
    _encoder.setCounts(0);
    _knobFraction = 0.0f;
}

void InputService::setButtonLed(bool on) {
    digitalWrite(BoardPins::PIN_SW_LED, on ? HIGH : LOW);
}

