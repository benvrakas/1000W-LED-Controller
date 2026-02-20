#include "PowerButton.h"

//Button Class Construction
    PowerButtonManager::PowerButtonManager(uint8_t swPin, uint8_t ledPin)
        : _swPin(swPin), _ledPin(ledPin), _pressTimeStamp(0),_pressDuration(0),  _armStatus(false)
    {}

//Button Functions Definition
    //Initialization
    void PowerButtonManager::begin() {
        pinMode(_swPin, INPUT_PULLUP);       
    }

    //Getters
    bool PowerButtonManager::isArmed() const {
        return _armStatus;
    }

    //ISR
    void PowerButtonManager::handleButtonInterrupt() {
        if (digitalRead(_swPin) == LOW) {
            _pressTimeStamp = millis();
            if (_armStatus == true) {
                _armStatus = false;
            }
        }
        
        if (digitalRead(_swPin) == HIGH) { 
            _pressDuration = millis() - _pressTimeStamp;           
            if (_pressDuration >= 3000) {
                _armStatus = true;
            }
        }
    }