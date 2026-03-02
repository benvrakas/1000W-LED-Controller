#include "Tachometers.h"
#include "PowerButton.h"
#include "Encoder.h"
#include <Arduino.h>

//Bridge to connect ISR to class methods
//ISR Bridge Tachometer Functions
    void pumpISR()    { pump.handleTachoInterrupt(); }
    void auxFanISR()  { auxFan.handleTachoInterrupt(); }
    void mainFanISR() { mainFan.handleTachoInterrupt(); }
    void psuFanISR()  { psuFan.handleTachoInterrupt(); }

//ISR Bridge Power Button Function
	void powerButtonISR() { powerButton.handleButtonInterrupt(); }

//ISR Bridge Encoder Functions
	void encoderAISR() { encoder.handleInterruptA(); }
	void encoderBISR() { encoder.handleInterruptB(); }
    
// NOTE: ISR attachment must happen after pins have been configured (pinMode).
// The attachInterrupt(...) calls were moved into the system startup sequence
// so they run after `boardPinsInit()` sets pin modes. Keeping this file as
// the ISR bridge implementation only.
