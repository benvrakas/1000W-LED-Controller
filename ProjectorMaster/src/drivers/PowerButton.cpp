#include "PowerButton.h"

//Button Class Construction
    PowerButtonManager::PowerButtonManager(uint8_t swPin, uint8_t ledPin)
        : _swPin(swPin), _ledPin(ledPin), _pressDuration(0), _buttonPressed(false), 
        _abortRequested(false), _lastState(false)
    {}

//Button Functions Definition
        //Initialization
        void PowerButtonManager::begin() {
            pinMode(_swPin, INPUT_PULLUP);
        }

        //Getters
        bool PowerButtonManager::isPressed() const {
            return _buttonPressed;
        }

        bool PowerButtonManager::isLongPress() const {
            return _pressDuration >= 3000; // 3 seconds threshold
        }

        //Monitoring

        //ISR
        void PowerButtonManager::handleButtonInterrupt() {
            if (digitalRead(_swPin) == LOW) {
                abortRequested = true;
            }
        }