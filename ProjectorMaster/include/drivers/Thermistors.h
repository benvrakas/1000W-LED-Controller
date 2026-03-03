#pragma once

#include <Arduino.h>
#include "config/ThermalConfig.h"

class ThermistorManager {
    public:    
        //Class Construction
        ThermistorManager(uint8_t pin, float beta, uint32_t seriesResistor);

        //Initialization
        void begin();

        //Getters
        float getCelsius() const;
        bool getStatus() const;

    private:
        uint8_t  _pin;
        float    _beta;
        uint32_t _seriesResistor;
        float    _currentTemp;
        bool     _isValid;

        //Caclulation
        float calculateCelsius(uint16_t adcValue);

        //Update Temperature Reading
        void updateTemp();
};
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;