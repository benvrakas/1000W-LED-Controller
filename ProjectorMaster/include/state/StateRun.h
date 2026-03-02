#pragma once

#include "state/SystemController.h"

class SystemRunning {
public:
    //Class Construction
    SystemRunning();

    //Initialization
    void begin();

    //Setters

    //Getters

private:
    bool _errorCase;

    //Telemetry Updates
        //Tachometers
        void checkTachometersSafety();

        //Thermistors
        void checkThermistorsSafety();
        
        //PSU
        void checkPSUSafety();
        
        //Display
        void checkDisplaySafety();
        
        //Power Button
        void checkPowerButtonInput();

        //Encoder
        void checkEncoderInput();
};

