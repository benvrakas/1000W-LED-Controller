#include "Thermistors.h"

//Thermistor Class Construction
    ThermistorManager::ThermistorManager(uint8_t pin, float beta, uint32_t seriesResistor)
        : _pin(pin), _beta(beta), _seriesResistor(seriesResistor),
          _currentTemp(0.0f), _isValid(false) 
    {}
    
//Thermistor Functions Definition
        //Initialization
        void ThermistorManager::begin() {
            pinMode(_pin, INPUT);
            // SAMD51 specific: ensure we are using 12-bit precision
            analogReadResolution(12);
        }

        //Getters
        float ThermistorManager::getCelsius() const {
            return _currentTemp;
        }

        bool ThermistorManager::getStatus() const {
            return _isValid;
        }

        //Calculation (Steinhart-Hart)
        float ThermistorManager::calculateCelsius(uint16_t adc) {
            // 1. Guard against divide-by-zero if sensor is shorted/grounded
            if (adc >= 4095 || adc <= 0) return 999.0f;

            // 2. Convert ADC to Resistance (MATCHES YOUR SCHEMATIC: R1/R2 to 3.3V)
            // Formula for Thermistor to GND: R = R_fixed * (ADC / (4095 - ADC))
            float resistance = (float)_seriesResistor * ((float)adc / (4095.0f - (float)adc));

            // 3. Steinhart-Hart Simplified (Beta Equation)
            float steinhart;
            steinhart = resistance / 10000.0f;     // (R/Ro)
            steinhart = logf(steinhart);           // ln(R/Ro) - using logf for SAMD51 FPU
            steinhart /= _beta;                    // 1/B * ln(R/Ro)
            steinhart += 1.0f / (25.0f + 273.15f); // + (1/To)
            steinhart = 1.0f / steinhart;          // Invert
            steinhart -= 273.15f;                  // Convert to Celsius

            return steinhart;
        }

        //Update Temperature Reading
        void ThermistorManager::updateTemp() {
            uint16_t raw = analogRead(_pin);
                
                if (raw > 4080 || raw < 15) {
                _isValid = false; 
                _currentTemp = 999.0f; // Force an error value for safety
                return; // Exit early; don't calculate or smooth garbage data
                }
            
            _isValid = true;

            float newTemp = calculateCelsius(raw);
            
                if (_currentTemp == 0.0f) { 
                    _currentTemp = newTemp;
                } else {
                    _currentTemp = (_currentTemp * 0.9f) + (newTemp * 0.1f);
                }
        }