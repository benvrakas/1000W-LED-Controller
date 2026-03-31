#pragma once

#include <Arduino.h>
#include "config/ThermalConfig.h"

class ThermistorManager {
    public:    
        //Class Construction
        ThermistorManager(uint8_t pin, float beta, uint32_t seriesResistor, float nominalResist, bool highSide);

        //Initialization
        void begin();

        //Getters
        float getCelsius() const;
        bool getStatus() const;
        uint16_t getRawADC() const { return _lastRawADC; }

        //Update Temperature Reading
        void updateTemp();

    private:
        uint8_t  _pin;
        float    _beta;
        uint32_t _seriesResistor;
        float    _nominalResistance;
        bool     _isHighSide;
        float    _currentTemp;
        bool     _isValid;
        uint16_t _lastRawADC;
        uint8_t  _errorCounter; // Instance-specific error counter for EMF protection

        //Caclulation
        float calculateCelsius(uint16_t adcValue);
};
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;