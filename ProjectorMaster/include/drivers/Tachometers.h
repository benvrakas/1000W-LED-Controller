#pragma once

#include <Arduino.h>

namespace TachometerConfig {
    //Deadstart Duty Cycles
    static constexpr uint8_t  MAIN_PSU_DEADSTART_DUTY = 77;
    static constexpr uint8_t  AUX_DEADSTART_DUTY = 26;
    static constexpr uint8_t  PUMP_DEADSTART_DUTY = 127;

    //Computation Intervals
    static constexpr unsigned long MAIN_COMPUTE_INTERVAL = 500;
    static constexpr unsigned long PSU_COMPUTE_INTERVAL = 100;
    static constexpr unsigned long AUX_COMPUTE_INTERVAL = 200;
    static constexpr unsigned long PUMP_COMPUTE_INTERVAL = 200;

    //RPM Limits
    static constexpr uint16_t MAX_MAIN_PSU_RPM = 3000;
    static constexpr uint16_t MAX_AUX_RPM = 6000;
    static constexpr uint16_t MAX_PUMP_RPM = 4800;

    //Stall Limits
    static constexpr uint16_t MAIN_PSU_STALL_RPM = 300;
    static constexpr uint16_t AUX_STALL_RPM = 300;
    static constexpr uint16_t PUMP_STALL_RPM = 150;

    //PID Constants
        //Radiator Fans
        static constexpr float MAIN_KP = 0.2f;
        static constexpr float MAIN_KI = 0.03f;
        static constexpr float MAIN_KD = 0.1f;

        //PSU Fan
        static constexpr float PSU_KP = 0.2f;
        static constexpr float PSU_KI = 0.03f;
        static constexpr float PSU_KD = 0.1f;

        //Pump
        static constexpr float PUMP_KP = 0.15f;
        static constexpr float PUMP_KI = 0.02f;
        static constexpr float PUMP_KD = 0.05f; 

    //Low Pass Filtter Alpha Values
    static constexpr float MAIN_ALPHA = 0.15f;
    static constexpr float PSU_ALPHA = 0.5f;
    static constexpr float PUMP_ALPHA = 0.3f;

    }

class TachometerManager {
    public:
        //Class Construction
        TachometerManager(uint8_t pwmPin, uint8_t tachPin, unsigned long computeInterval, 
            float kp, float ki, float kd, float alpha, uint8_t minDeadStart, uint16_t maxRPM, uint16_t stallRPM);

        //Initialization
        void begin();

        //Getters
        uint16_t getRPM() const; 
        uint8_t getDuty() const;
        uint32_t getPulseCount() const;
        bool getStallStatus() const; // Returns true if PWM > 0 but RPM is 0

        //Emergency Stops
        void stop();                
        void stopSlow();

        //ISR Support (Needs to be public or handled via a static wrapper)
        void handleTachoInterrupt();

    private:
        //Hardware Pins
        uint8_t _pwmPin;
        uint8_t _tachPin;
        
        //Constants
        unsigned long _computeInterval;

            //Motor
            uint8_t _minDeadStart;
            uint16_t _maxRPM;
            uint16_t _stallRPM;

            //PID
            float _kp;
            float _ki;
            float _kd;
            float _alpha;

        //Operational Variables
            //Motor
            uint8_t  _currentDuty;
            volatile uint32_t _pulseCount; // volatile because it changes in interrupt
            uint16_t _currentRPM;

            //PID
            float _integral;
            float _lastError;
            float _lastPID;
            float _filteredDerivative;
            
            //Last Computation Times
            unsigned long _lastRPMCompute;
            unsigned long _lastPWMCompute;
            unsigned long _lastPIDCompute;

        //Calculation
        void calculateRPM(unsigned long currentMillis);
        void calculatePWM(unsigned long currentMillis);
        void calculatePID(unsigned long currentMillis, uint16_t sv, uint16_t pv);
        
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