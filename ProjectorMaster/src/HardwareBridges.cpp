#include <Arduino.h>
#include "Tachometers.h"

//Bridge to connect ISR to class methods

//ISR Bridge Tachometer Functions
    void pumpISR()    { pump.handleTachoInterrupt(); }
    void auxFanISR()  { auxFan.handleTachoInterrupt(); }
    void mainFanISR() { mainFan.handleTachoInterrupt(); }
    void psuFanISR()  { psuFan.handleTachoInterrupt(); }