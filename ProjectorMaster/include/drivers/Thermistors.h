#pragma once

#include <Arduino.h>

namespace ThermistorConfig {
    static constexpr float MAX_TEMP_LED = 75.0f;
    static constexpr float MAX_TEMP_PUMP = 50.0f;

    static constexpr float BETA_VALUE_LED = 3950.0f;
    static constexpr float BETA_VALUE_PUMP = 3950.0f;

    static constexpr uint32_t SERIES_RESISTOR_LED = 10000UL; // 10k Ohm
    static constexpr uint32_t SERIES_RESISTOR_PUMP = 10000UL; // 12-bit ADC
}

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