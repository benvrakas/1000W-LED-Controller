#pragma once

#include <Arduino.h>

class SystemController; // Forward declaration

class SystemStartup {
public:
    //Class Construction
    SystemStartup();

    //Getter for checking each step status
    bool getStepStatus(uint8_t bootStep) const;

    //Setter for each step status
    void setStepStatus(uint8_t bootStep, bool status);

    //Initialization and verification functions
    void boardPinsInit(SystemController& sys);
    void boardPinsVerify(uint8_t bootStep);
    void isrInit();
    void pumpInit(SystemController& sys);
    void pumpVerify(uint8_t bootStep);
    void fansInit(SystemController& sys);
    void fansVerify(uint8_t bootStep);
    void psuInit(SystemController& sys);
    void psuVerify(uint8_t bootStep);
    void displayInit(SystemController& sys);
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

