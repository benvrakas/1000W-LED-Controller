#pragma once

#include <Arduino.h>

class PowerButtonManager {
    public:
        //Class Construction
        PowerButtonManager(uint8_t swPin, uint8_t ledPin);

        //Initialization
        void begin();

        //Getters
        // For v1, "armed" is equivalent to "system ON".
        bool isArmed() const;

        // Call from the main loop/state machine with the current time.
        // This handles long-press detection for ON and debounced OFF.
        void update(unsigned long now);

        //ISR Support
        void handleButtonInterrupt();
        
    private:
        uint8_t  _swPin;
        uint8_t  _ledPin;
        volatile unsigned long _pressTimeStamp;
        volatile unsigned long _pressDuration;
        volatile bool _buttonPressed; // true while physically held down
        volatile bool _armStatus;      // true = ON, false = OFF
        bool _armToggleDone;           // prevents re-triggering while held
        bool _wasPressed;              // edge detector for release logic

};
extern PowerButtonManager powerButton;

//ISR Bridge Function Declarations
void powerButtonISR();
