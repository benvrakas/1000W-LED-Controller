#pragma once

#include <Arduino.h>

namespace TachometerConfig {
    static constexpr uint8_t  MIN_AUX_DEADSTART_DUTY   = 26; // Minimum duty cycle to overcome static friction
    static constexpr uint8_t  MIN_MAIN_PSU_DEADSTART_DUTY   = 77; // Minimum duty cycle to overcome static friction
}

class TachometerManager {
    public:
        //Initialization
        TachometerManager(uint8_t pwmPin, uint8_t tachPin);
        void begin();

        //Setters
        void setDuty(uint8_t duty, uint8_t minDeadStart); // 0-255

        //Getters
        uint16_t getRPM() const; 
        uint8_t getDuty() const;
        uint32_t getPulseCount() const;

        //RPM Calculation
        void calculateRPM(unsigned long currentMillis);

        //Emergency Stops
        void stop();                
        void stopSlow();

        //Monitoring    
        bool isStalled() const; // Returns true if PWM > 0 but RPM is 0

        //PID Calculation
        float tunePID(float sv, float pv);

        //ISR Support (Needs to be public or handled via a static wrapper)
        void handleTachoInterrupt();

private:
    uint8_t _pwmPin;
    uint8_t _tachPin;
    uint8_t  _currentDuty;
    volatile uint32_t _pulseCount; // volatile because it changes in interrupt
    uint16_t _currentRPM;
    unsigned long _lastRPMCompute;
    
    //Safety thresholds
    const uint32_t _computeInterval = 100; // Compute RPM every 1s
    const uint16_t _stallThreshold  = 100;  // RPM below this is a "stall"

    // PID State variables
    float _integral;
    float _lastError;
    uint32_t _lastPIDTime;
    float _lastOutput;

    // PID Constants (Safe startup values)
    const float _Kp = 0.2f;
    const float _Ki = 0.03f;
    const float _Kd = 0.1f;
};

//Extern declarations for global TachometerManager instances
extern TachometerManager pump;
extern TachometerManager auxFan;
extern TachometerManager mainFan;
extern TachometerManager psuFan;

//ISR Bridge Function Declarations
void pumpISR();
void auxFanISR();
void mainFanISR();
void psuFanISR();