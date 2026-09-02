#include "PowerButton.h"
#include <Arduino.h>

PowerButtonManager::PowerButtonManager(uint8_t swPin, uint8_t ledPin)
    : _swPin(swPin), _ledPin(ledPin), _pressTimeStamp(0), _pressDuration(0),
      _buttonPressed(false), _armStatus(false), _armToggleDone(false), _wasPressed(false) {}

void PowerButtonManager::begin() {
    pinMode(_swPin, INPUT_PULLUP);
    // Initial state check
    bool pressed = (digitalRead(_swPin) == LOW);
    _buttonPressed = pressed;
    _wasPressed = pressed;
}

bool PowerButtonManager::isArmed() const {
    return _armStatus;
}

void PowerButtonManager::update(unsigned long now) {
    // Read the physical state via polling for debounced stability
    bool isDown = (digitalRead(_swPin) == LOW);

    if (isDown) {
        if (!_wasPressed) {
            // PRESS EDGE
            // Check the lockout against the PREVIOUS timestamp (set when we
            // armed, or by our last press) before overwriting it below --
            // doing it in the other order made "now - _pressTimeStamp"
            // always ~0, so this could never fire and the unit could never
            // be disarmed once armed.
            if (_armStatus && (now - _pressTimeStamp > 500)) {
                // If system is already ON, any new press is an instant OFF
                // Added 500ms lockout to prevent noise from PSU start-up disarming us
                _armStatus = false;
                _armToggleDone = true;
                Serial.println(F("BTN: DISARMED (User Press)"));
            }

            _pressTimeStamp = now;
        } else {
            // CONTINUOUS HOLD
            if (!_armStatus && !_armToggleDone) {
                if (now - _pressTimeStamp >= 3000UL) {
                    _armStatus = true;
                    _armToggleDone = true; 
                    _pressTimeStamp = now; // Reset timestamp to start the 500ms lockout
                    Serial.println(F("BTN: ARMED (3s Hold)"));
                }
            }
        }
    } else {
        // RELEASED
        _armToggleDone = false;
    }

    _wasPressed = isDown;
}

void PowerButtonManager::handleButtonInterrupt() {
    // ISR can stay empty or just update a flag; we are using polling in update() 
    // which runs at high frequency in the state machine loop.
}
