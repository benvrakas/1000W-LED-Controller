#include "PowerButton.h"

// Button Class Construction
PowerButtonManager::PowerButtonManager(uint8_t swPin, uint8_t ledPin)
    : _swPin(swPin), _ledPin(ledPin), _pressTimeStamp(0), _pressDuration(0),
      _buttonPressed(false), _armStatus(false) {}

// Button Functions Definition
// Initialization
void PowerButtonManager::begin() {
    pinMode(_swPin, INPUT_PULLUP);
}

// Getters
bool PowerButtonManager::isArmed() const {
    return _armStatus;
}

// Polling-style update, called from the main loop/state machine
void PowerButtonManager::update(unsigned long now) {
    // If button is held continuously for >= 3000 ms, arm (ON)
    if (_buttonPressed) {
        unsigned long heldMs = now - _pressTimeStamp;
        if (!_armStatus && heldMs >= 3000UL) {
            _armStatus = true;
        }
    } else {
        // On release, if we were armed (ON), treat this as an OFF request.
        // OFF is logical only; the LED current slew-down is handled by
        // higher-level PSU control.
        if (_armStatus) {
            _armStatus = false;
        }
    }
}

// ISR: track physical button press/release only
void PowerButtonManager::handleButtonInterrupt() {
    int level = digitalRead(_swPin);
    unsigned long now = millis();

    if (level == LOW) {
        // Button just pressed
        _buttonPressed  = true;
        _pressTimeStamp = now;
    } else {
        // Button released
        _buttonPressed   = false;
        _pressDuration   = now - _pressTimeStamp;
    }
}
