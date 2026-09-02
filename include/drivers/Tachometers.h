#pragma once

#include <Arduino.h>
#include "config/ThermalConfig.h"

// ---------------------------------------------------------------------------
// TachometerManager - PWM output + tachometer RPM feedback (no PID)
// Fan/pump speed is controlled via setDuty() based on external fan curves.
// ---------------------------------------------------------------------------
class TachometerManager {
public:
    // Constructor: pwmPin, tachPin, minDeadStart duty, maxRPM, stallRPM threshold
    // fixedPwmFreqHz: 0 = use the Arduino core's default PWM frequency
    // (~1.8kHz on SAMD51 @ 120MHz, prescaler /256 over an 8-bit period).
    // Some fans need an exact PWM frequency for their input decode circuit
    // and default to full speed if the signal doesn't lock (e.g. the NMB
    // 12038VA datasheet specs PWM Frequency f = 25kHz and documents
    // "Vst = Open -> Full Speed" as the fallback). Only pins mapped to a
    // plain 8-bit TC timer (not TCC) are supported; see .cpp for why.
    TachometerManager(uint8_t pwmPin, uint8_t tachPin,
                      uint8_t minDeadStart, uint16_t maxRPM, uint16_t stallRPM,
                      uint32_t fixedPwmFreqHz = 0);

    // Initialize PWM and tach pins
    void begin();

    // Set PWM duty cycle (0–255)
    void setDuty(uint8_t duty);

    // Getters
    uint16_t getRPM() const;
    uint8_t  getDuty() const;
    uint32_t getPulseCount() const;
    bool     getStallStatus() const;  // true if duty > 0 but RPM below stall threshold

    // Highest RPM seen since duty last went 0->nonzero -- "how well did this
    // channel actually spin up," independent of whatever its duty has been
    // set to since (e.g. ERROR_KILL's failsafe zeroing it back out). Reset
    // on the same 0->nonzero transition as resetStallHistory().
    uint16_t getPeakRPM() const;

    // RPM averaged over TachometerConfig::STALL_EVAL_WINDOW_MS rather than a
    // single RPM_COMPUTE_INTERVAL sample. This is what stall detection
    // judges against -- see STALL_EVAL_WINDOW_MS for why a single 200ms
    // sample is too coarse near the stall threshold. Falls back to the
    // latest sample until the window has filled.
    uint16_t getWindowedRPM() const;

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
    uint32_t _fixedPwmFreqHz;

    // Fixed-frequency PWM state (only set when _fixedPwmFreqHz > 0 and the
    // pin resolves to a plain TC timer). Null Tc* means setDuty() falls
    // back to the normal analogWrite() path.
    Tc*     _fixedFreqTc;
    uint8_t _fixedFreqChannel;
    uint8_t _fixedFreqPeriod; // COUNT8 PER value (max compare count)

    // Number of RPM_COMPUTE_INTERVAL samples that make up the stall
    // evaluation window. At the defaults that's 1000/200 = 5.
    static constexpr uint8_t STALL_HISTORY_SLOTS =
        TachometerConfig::STALL_EVAL_WINDOW_MS / TachometerConfig::RPM_COMPUTE_INTERVAL;

    // Runtime state
    uint8_t  _currentDuty;
    volatile uint32_t _pulseCount;  // incremented in ISR
    uint16_t _currentRPM;
    uint16_t _peakRPM;
    unsigned long _lastRPMCompute;
    unsigned long _dutyOnSinceMs; // 0 when duty is 0; set when duty goes 0->nonzero

    // Rolling history backing getWindowedRPM(). Pulses and durations are
    // kept separately rather than pre-divided per-sample so the window sums
    // exactly (sum(pulses) over sum(durations)) instead of averaging away
    // the resolution we're trying to recover.
    uint32_t _histPulses[STALL_HISTORY_SLOTS];
    uint32_t _histDurationMs[STALL_HISTORY_SLOTS];
    uint8_t  _histIndex;
    uint8_t  _histCount;   // valid slots, saturating at STALL_HISTORY_SLOTS

    // Drops every sample in the rolling window. Called when duty goes
    // 0->nonzero so samples taken while the rotor was deliberately stopped
    // can't drag the windowed RPM down after a restart.
    void resetStallHistory();

    // Internal RPM calculation
    void calculateRPM(unsigned long currentMillis);

    // Reconfigures the pin's timer for _fixedPwmFreqHz instead of the core
    // default. No-op (leaves default frequency in place) if the pin maps to
    // a TCC instead of a plain TC.
    void configureFixedFrequencyPwm();

    // Writes duty (0-255) to the PWM output, scaling to _fixedFreqPeriod via
    // direct timer register access when configureFixedFrequencyPwm() applied,
    // otherwise via the normal analogWrite() path.
    void writeDutyToTimer(uint8_t duty);
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