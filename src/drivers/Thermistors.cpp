#include "Thermistors.h"
#include <Arduino.h>

ThermistorManager::ThermistorManager(uint8_t pin, float beta, uint32_t seriesResistor, float nominalResist, bool highSide)
    : _pin(pin), _beta(beta), _seriesResistor(seriesResistor), _nominalResistance(nominalResist),
      _isHighSide(highSide), _currentTemp(0.0f), _isValid(false), _lastRawADC(0), _errorCounter(0) 
{}

void ThermistorManager::begin() {
    pinMode(_pin, INPUT);
}

float ThermistorManager::getCelsius() const {
    return _currentTemp;
}

bool ThermistorManager::getStatus() const {
    return _isValid;
}

float ThermistorManager::calculateCelsius(uint16_t adc) {
    if (adc >= 4095 || adc <= 1) return 999.0f;

    float resistance;
    if (_isHighSide) {
        resistance = (float)_seriesResistor * ((4095.0f - (float)adc) / (float)adc);
    } else {
        resistance = (float)_seriesResistor * ((float)adc / (4095.0f - (float)adc));
    }

    float steinhart;
    steinhart = resistance / _nominalResistance; 
    steinhart = logf(steinhart);           
    steinhart /= _beta;                    
    steinhart += 1.0f / (25.0f + 273.15f); 
    steinhart = 1.0f / steinhart;          
    steinhart -= 273.15f;                  

    return steinhart;
}

void ThermistorManager::updateTemp() {
    uint32_t acc = 0;
    for(int i=0; i<32; i++) {
        acc += analogRead(_pin);
    }
    uint16_t raw = acc >> 5; 
    _lastRawADC = raw;
        
    if (raw > 4090 || raw < 5) {
        _errorCounter++;
        if (_errorCounter > 20) {
            _isValid = false; 
            _currentTemp = 999.0f; 
        }
        return; 
    }
    _errorCounter = 0;
    
    _isValid = true;
    float newTemp = calculateCelsius(raw);
    
    if (_currentTemp <= 0.0f || _currentTemp >= 998.0f) { 
        _currentTemp = newTemp;
    } else {
        _currentTemp = (_currentTemp * 0.9f) + (newTemp * 0.1f);
    }
}
