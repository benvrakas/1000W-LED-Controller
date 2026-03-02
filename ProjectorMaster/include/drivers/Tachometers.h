#pragma once

#include <Arduino.h>

namespace TachometerConfig {
    // Deadstart Duty Cycles (minimum PWM to start spinning)
    static constexpr uint8_t  MAIN_PSU_DEADSTART_DUTY = 77;
    static constexpr uint8_t  AUX_DEADSTART_DUTY = 26;
    static constexpr uint8_t  PUMP_DEADSTART_DUTY = 127;

    // RPM Computation Intervals (milliseconds)
    static constexpr unsigned long RPM_COMPUTE_INTERVAL = 200;

    // RPM Limits (for scaling/clamping)
    static constexpr uint16_t MAX_MAIN_PSU_RPM = 3000;
    static constexpr uint16_t MAX_AUX_RPM = 6000;
    static constexpr uint16_t MAX_PUMP_RPM = 4800;

    // Stall Detection Thresholds (RPM below this with duty > 0 = stall)
    static constexpr uint16_t MAIN_PSU_STALL_RPM = 300;
    static constexpr uint16_t AUX_STALL_RPM = 300;
    static constexpr uint16_t PUMP_STALL_RPM = 150;
}

// ---------------------------------------------------------------------------
// TachometerManager - PWM output + tachometer RPM feedback (no PID)
// Fan/pump speed is controlled via setDuty() based on external fan curves.
// ---------------------------------------------------------------------------
class TachometerManager {
public:
    // Constructor: pwmPin, tachPin, minDeadStart duty, maxRPM, stallRPM threshold
    TachometerManager(uint8_t pwmPin, uint8_t tachPin,
                      uint8_t minDeadStart, uint16_t maxRPM, uint16_t stallRPM);

    // Initialize PWM and tach pins
    void begin();

    // Set PWM duty cycle (0–255)
    void setDuty(uint8_t duty);

    // Getters
    uint16_t getRPM() const;
    uint8_t  getDuty() const;
    uint32_t getPulseCount() const;
    bool     getStallStatus() const;  // true if duty > 0 but RPM below stall threshold

    // Emergency stops
    void stop();       // Immediate stop (duty = 0)
    void stopSlow();   // Gradual ramp-down

    // ISR callback (public for static wrapper)
    void handleTachoInterrupt();

    // Call from main loop to update RPM calculation
    void update(unsigned long currentMillis);

private:
    // Hardware pins
    uint8_t _pwmPin;
    uint8_t _tachPin;

    // Configuration
    uint8_t  _minDeadStart;
    uint16_t _maxRPM;
    uint16_t _stallRPM;

    // Runtime state
    uint8_t  _currentDuty;
    volatile uint32_t _pulseCount;  // incremented in ISR
    uint16_t _currentRPM;
    unsigned long _lastRPMCompute;

    // Internal RPM calculation
    void calculateRPM(unsigned long currentMillis);
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