#pragma once

#include <Arduino.h>

namespace ThermistorConfig {
    static constexpr float MAX_TEMP_LED = 75.0f;
    static constexpr float MAX_TEMP_PUMP = 50.0f;
}

class ThermistorManager {
    public:       
        // Initialization
        ThermistorManager(uint8_t pin, float beta, uint32_t seriesResistor);
        void begin();

        // Getters for your SystemController
        float getCelsius() const;
        bool isActive() const;

        // The "Worker" function: Call this in your loop to refresh data
        void updateTemp();

    private:
        uint8_t  _pin;
        float    _beta;
        uint32_t _seriesResistor;
        float    _currentTemp;
        bool     _isValid;

        // Internal math for Steinhart-Hart
        float calculateCelsius(uint16_t adcValue);
};
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;