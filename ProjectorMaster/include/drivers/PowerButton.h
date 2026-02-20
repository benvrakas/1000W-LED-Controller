#pragma once

#include <Arduino.h>

class PowerButtonManager {
    public:
        //Class Construction
        PowerButtonManager(uint8_t swPin, uint8_t ledPin);

        //Initialization
        void begin();

        //Getters
        bool isArmed() const;

        //ISR Support
        void handleButtonInterruptRising();
        void handleButtonInterruptFalling();
        
    private:
        uint8_t  _swPin;
        uint8_t  _ledPin;
        volatile unsigned long _pressTimeStamp;
        volatile unsigned long _pressDuration;
        volatile bool _armStatus;

};
extern PowerButtonManager powerButton;

//ISR Bridge Function Declarations
void powerButtonISRRising();
void powerButtonISRFalling();