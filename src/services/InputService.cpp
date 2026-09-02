#include "services/InputService.h"
#include "config/PinMap.h"
#include <Arduino.h>

InputService::InputService(EncoderManager& encoder, PowerButtonManager& powerButton)
    : _encoder(encoder), _powerButton(powerButton),
      _armed(false),
      _prevArmed(false),
      _edgeArmedOn(false),
      _edgeArmedOff(false),
      _knobFraction(0.0f) {}

void InputService::begin() {
    // Seed state from current button status
    _armed     = _powerButton.isArmed();
    _prevArmed = _armed;

    // Reset encoder/knob to 0 at initialization
    forceKnobToZero();

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

    // Update encoder-derived knob fraction. Only tracked while armed --
    // while disarmed the knob has no effect on output, so the on-screen
    // gauge shouldn't visibly move either (turning it while off used to
    // silently roll _knobFraction even though nothing downstream applied
    // it, which looked like the setpoint was adjustable while off).
    if (currentArmed) {
        int16_t counts = _encoder.getCounts();
        if (counts < 0) counts = 0;
        if (counts > EncoderManager::MAX_COUNTS_RANGE) {
            counts = EncoderManager::MAX_COUNTS_RANGE;
        }

        _knobFraction = static_cast<float>(counts) /
                        static_cast<float>(EncoderManager::MAX_COUNTS_RANGE);
    } else {
        _knobFraction = 0.0f;
    }
}

void InputService::forceKnobToZero() {
    _encoder.setCounts(0);
    _knobFraction = 0.0f;
}

void InputService::setButtonLed(bool on) {
    digitalWrite(PinMap::PIN_SW_LED, on ? HIGH : LOW);
}

