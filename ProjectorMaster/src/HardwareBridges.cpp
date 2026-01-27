#include "Tachometers.h"
#include "PowerButton.h"
#include "Encoder.h"


//Bridge to connect ISR to class methods
//ISR Bridge Tachometer Functions
    void pumpISR()    { pump.handleTachoInterrupt(); }
    void auxFanISR()  { auxFan.handleTachoInterrupt(); }
    void mainFanISR() { mainFan.handleTachoInterrupt(); }
    void psuFanISR()  { psuFan.handleTachoInterrupt(); }

//ISR Bridge Power Button Function
    void powerButtonISR() { powerButton.handleButtonInterrupt(); }