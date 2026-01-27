#include "Tachometers.h"

//Class Construction
TachometerManager::TachometerManager(uint8_t pwmPin, uint8_t tachPin, unsigned long computeInterval, 
    float kp, float ki, float kd, float alpha, uint8_t minDeadStart, uint16_t maxRPM, uint16_t stallRPM)
        : _pwmPin(pwmPin), _tachPin(tachPin), _computeInterval(computeInterval), _kp(kp), _ki(ki), 
        _kd(kd), _alpha(alpha), _minDeadStart(minDeadStart), _maxRPM(maxRPM), _stallRPM(stallRPM),
        _currentDuty(0), _pulseCount(0), _currentRPM(0), _integral(0), _lastError(0), _lastPID(0), 
        _filteredDerivative(0.0f), _lastRPMCompute(0), _lastPWMCompute(0), _lastPIDCompute(millis())
    {}

//Functions Definition
        //Initialization
        void TachometerManager::begin() {
            pinMode(_pwmPin, OUTPUT);
            pinMode(_tachPin, INPUT_PULLUP);
            analogWriteResolution(8);
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
            count = _pulseCount;
            return count;
        }
        
        bool TachometerManager::getStallStatus() const {
            return (_currentDuty > 0 && _currentRPM <= _stallRPM);
        }

        //Emergency Stops

        //ISR Support
        void TachometerManager::handleTachoInterrupt() {
            _pulseCount++;
        }

        //Calculation
        void TachometerManager::calculatePWM(unsigned long currentMillis) {
            unsigned long now = currentMillis;
            unsigned long duration = now - _lastPWMCompute;

            if (duration >= _computeInterval) {
                uint8_t duty = static_cast<uint8_t>((_lastPID * 255.0f) + 0.5f); // Round to nearest integer
                
                if (duty > 0 && duty < _minDeadStart) {
                    duty = _minDeadStart;
                }
    
                _currentDuty = duty;
                _lastPWMCompute = now;
                analogWrite(_pwmPin, _currentDuty);                
            }   
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

        void TachometerManager::calculatePID(unsigned long currentMillis, uint16_t sv, uint16_t pv) {

            unsigned long now = currentMillis;
            unsigned long duration = now - _lastPIDCompute;
            
            float output = 1.0f;
            float rpmFactor = 1.0f;
            float error = 0.0f;

            if (duration >= _computeInterval) {
                error = pv - sv;
                float dt = (now - _lastPIDCompute) / 1000.0f; // Convert to seconds

                // Guard against double-calling or 0-time steps
                if (dt <= 0.0f) {
                    output = _lastPID;            
                } else {
                    // 1. Proportional term
                    rpmFactor += (_currentRPM / 10000.0f);
                    float Pout = (_kp * rpmFactor) * error;

                    // 2. Integral term
                    _integral += error * dt;
                    
                    // Calculate the maximum the accumulator can reach so that (ki * _integral) == 0.25
                    float maxAccumulator = 0.25f / _ki; 

                    // Windup Guard: Limits the resulting Iout to exactly 0.25 (25% power)
                    if (_integral > maxAccumulator) { _integral = maxAccumulator; } 
                    else if (_integral < 0.0f) { _integral = 0.0f; }
                    
                    float Iout = _ki * _integral;

                    // 3. Derivative term: Run through low-pass filter
                    float rawDerivative = (error - _lastError) / dt;
                    _filteredDerivative = (_alpha * rawDerivative) + ((1.0f - _alpha) * (_filteredDerivative));
                    float Dout = _kd * _filteredDerivative;

                    // Calculate total
                    output = Pout + Iout + Dout;
                    if (output > 1.0f) {output = 1.0f;} 
                    else if (output < 0.0f) {output = 0.0f;}
                }
            // Save for next loop
            _lastError = error;
            _lastPIDCompute = now;
            _lastPID = output; 
            }
        }