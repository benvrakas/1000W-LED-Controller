#pragma once

#include <Arduino.h>
#include "BoardPins.h"

namespace ThermistorConfig {
    static constexpr float MAX_TEMP_LED = 75.0f;
    static constexpr float MAX_TEMP_PUMP = 50.0f;
}

class ThermistorManager {
public:
    // Constructor: Define the pin and the Beta coefficient of your thermistor
    ThermistorManager(uint8_t pin, float beta, uint32_t seriesResistor);
    
    // Initialization
    void begin();

    // Getters for your SystemController
    float getCelsius() const { return _currentTemp; }
    bool isActive() const { return _isValid; }

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