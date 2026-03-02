#pragma once

#include <Arduino.h>

class SystemStartup {
public:
    //Class Construction
    SystemStartup();

    //Getter for checking each step status
    bool getStepStatus(uint8_t bootStep) const;

    //Setter for each step status
    void setStepStatus(uint8_t bootStep, bool status);

    //Initialization and verification functions
    void boardPinsInit();
    void boardPinsVerify(uint8_t bootStep);
    void isrInit();
    void pumpInit();
    void pumpVerify(uint8_t bootStep);
    void fansInit();
    void fansVerify(uint8_t bootStep);
    void psuInit();
    void psuVerify(uint8_t bootStep);
    void displayInit();
    void displayVerify(uint8_t bootStep);

private:
    //Systems Checks
    bool _boardPinsReady;
    bool _pumpReady;
    bool _fansReady;
    bool _psuReady;
    bool _displayReady;
    bool _encoderReady;
    bool _thermistorsReady;

    //Helper to verify pin 
    bool isPinSetAsOutput(uint8_t pin) const;
    bool isPinSetAsInput(uint8_t pin) const;
};

