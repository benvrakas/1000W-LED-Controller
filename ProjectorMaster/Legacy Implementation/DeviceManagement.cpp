#include "BoardPins.h"
#include "OLED.h"
#include "PMBus.h"
#include "Tachometers.h"
#include "Encoder.h"
#include "Thermistors.h"
#include "PowerButton.h"

//Define Functions/Classes for device setup and modulation.
//Not every "Hardware device" needs a class some just need global functions

    //PSU Class Construction
    PMBusManager::PMBusManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address)
        : _address(address), _bus(nullptr) 
    {}

    //PSU Functions Definition
        //Initialization
        void PMBusManager::begin(TwoWire* i2cBus) {
            _bus = i2cBus;
        }

        
    //Display Class Construction
    OledManager::OledManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address, uint8_t width, uint8_t height)
        : _address(address), _width(width), _height(height), _bus(nullptr), _display(nullptr) 
    {}

    //Display Functions Definition
        //Initialization
        void OledManager::begin(TwoWire* i2cBus) {
            _bus = i2cBus;
            _display = new Adafruit_SSD1306(_width, _height, _bus, -1);

        }
    
    //Tachometer Class Construction
    TachometerManager::TachometerManager(uint8_t pwmPin, uint8_t tachPin)
        : _pwmPin(pwmPin), _tachPin(tachPin), _pulseCount(0), _lastRPMCompute(0),
          _currentRPM(0), _currentDuty(0) 
    {}

    //Tachometer Functions Definition
        //Initialization
        void TachometerManager::begin() {
            pinMode(_pwmPin, OUTPUT);
            pinMode(_tachPin, INPUT_PULLUP);
            analogWriteResolution(8);
        }
    
        //Setters
        void TachometerManager::setDuty(uint8_t duty, uint8_t minDeadStart) {
            if (_currentDuty == 0) {
                analogWrite(_pwmPin, minDeadStart); // Overcome static friction
            }
            _currentDuty = duty;
            // Direct register write or analogWrite
            analogWrite(_pwmPin, duty);
        }

        //Getters
        uint16_t TachometerManager::getRPM() const {
            return _currentRPM;

        }

        uint8_t TachometerManager::getDuty() const {
            return _currentDuty;
        }

        uint32_t TachometerManager::getPulseCount() const {
            uint32_t count;
            noInterrupts();
            count = _pulseCount;
            interrupts();
            return count;
        }

        //Calculation
        uint8_t TachometerManager::calculatePWM(unsigned long currentMillis, float pidOutput) {
            unsigned long now = currentMillis;
            unsigned long duration = now - _lastPWMCompute;

            if (duration > _computeInterval) {
            uint8_t pwmValue = static_cast<uint8_t>((pidOutput * 255.0f) + 0.5f); // Round to nearest integer
            _lastPWMCompute = now;    
            }     
            return pwmValue;  
        }

        void TachometerManager::calculateRPM(unsigned long currentMillis) {
            unsigned long now = currentMillis;
            unsigned long duration = now - _lastRPMCompute;

            if (duration >= _computeInterval) {
                uint32_t capturedPulses = _pulseCount; 
                _pulseCount = 0;                      

                _currentRPM = (uint16_t)((capturedPulses * 30000UL) / duration);
                
                _lastRPMCompute = now;
            }
        }

        float TachometerManager::tunePID(unsigned long currentMillis, uint16_t rpm, float sv, float pv) {
            float error = pv - sv;
            unsigned long now = currentMillis;
            float dt = (now - _lastPIDTime) / 1000.0f; // Convert to seconds

            // Guard against double-calling or 0-time steps
            if (dt <= 0.0f) return _lastPWM; 

            // 1. Proportional term
            float Pout = _Kp * error;

            // 2. Integral term (with windup protection for 1500W safety)
            _integral += error * dt;
            
            // Windup Guard: Limit the integral's influence to 25% of total power
            if (_integral > 1.0f) {
                _integral = 1.0f;
            } else if (_integral < 0.0f) {
                _integral = 0.0f;
            }

            float Iout = _Ki * _integral;

            // 3. Derivative term
            float derivative = (error - _lastError) / dt;
            float Dout = _Kd * derivative;

            // Calculate total
            float output = Pout + Iout + Dout;
            if (output > 1.0f) {
                output = 1.0f;
            } else if (output < 0.0f){
                output = 0.0f;
            }

            // Save for next loop
            _lastError = error;
            _lastPIDTime = now;
            _lastPWM = output; 

            return output;
        }

        //Emergency Stops
        void TachometerManager::stop() {

        }              

        void TachometerManager::stopSlow() {

        }

        //Monitoring    
        bool TachometerManager::isStalled() const {

        }

        //ISR Support (Needs to be public or handled via a static wrapper)
        void TachometerManager::handleTachoInterrupt() {
            _pulseCount++;
        }
    
    //Button Class Construction
    PowerButtonManager::PowerButtonManager(uint8_t swPin, uint8_t ledPin)
        : _pressDuration(0), 
        _buttonPressed(false), 
        _lastState(false),
        _abortRequested(false) 
    {}

    //Button Functions Definition
        //Initialization
        void PowerButtonManager::begin() {
            pinMode(BoardPins::PIN_SW_BTN, INPUT_PULLUP);
        }

        //Getters
        bool PowerButtonManager::isPressed() const {
            return _buttonPressed;
        }

        bool PowerButtonManager::isLongPress() const {
            return _pressDuration >= 3000; // 3 seconds threshold
        }

        //Monitoring

        //ISR
        void PowerButtonManager::handleButtonInterrupt() {
            if (digitalRead(BoardPins::PIN_SW_BTN) == LOW) {
                abortRequested = true;
            }
        }

    //Encoder Functions Definition
    
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

        //Setters
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

        //Getters
        float ThermistorManager::getCelsius() const {
            return _currentTemp;
        }

        bool ThermistorManager::isActive() const {
            return _isValid;
        }

        //Internal math for Steinhart-Hart
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