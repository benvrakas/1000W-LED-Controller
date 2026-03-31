#pragma once

#include <Arduino.h>

// PsuService
// ----------
// High-level wrapper around the PSU CAN manager. This service owns the
// notion of LED power setpoint, PSU operating mode, and interpreted PSU
// telemetry that other subsystems (cooling, UI, logging) can consume.

class CanBusManager;

class PsuService {
public:
    PsuService(CanBusManager& psu);

    // Bring the PSU control layer online. Intended to be called once from
    // the RUN state when the system first transitions from INIT.
    void begin();

    // Request PSU to turn on (enables CAN operation + remote gate)
    void requestOn();

    // Request PSU to turn off (disables CAN operation + remote gate when safe)
    void requestOff();

    // Set the desired UI setpoint as a 0..1 fraction. Typically sourced
    // from InputService (encoder position) before calling update().
    void setUiSetpointFraction(float frac) { _uiSetpointFrac = frac; }

    // Periodic update from RUN. Responsible for slewing the applied
    // setpoint toward the requested UI setpoint, issuing CAN commands,
    // and allowing the CAN manager to process telemetry/watchdog.
    void update(unsigned long now);

    // Getters for UI/other subsystems
    float getUiSetpointFraction() const      { return _uiSetpointFrac; }
    float getAppliedCurrentFraction() const  { return _appliedCurrentFrac; }
    bool  isOn() const { return _isOn; }

    // PSU telemetry (read from CAN)
    float getVoltage() const;
    float getCurrent() const;
    float getPower() const;
    bool  telemetryValid() const;

    // Slew configuration: standard operation vs shutdown. These values are
    // in percent-per-second and are passed into the internal slew function.
    static constexpr float SLEW_RATE_NORMAL_PCT_PER_SEC   = 30.0f;   // 30%/s
    static constexpr float SLEW_RATE_SHUTDOWN_PCT_PER_SEC = 100.0f;  // 100%/s

    // Allow higher-level logic (e.g. RUN state) to adjust the active slew
    // rate. Example usage:
    //  - NORMAL when system is running.
    //  - SHUTDOWN when user requests OFF so current can ramp down quickly.
    void setSlewRatePercentPerSec(float rate) { _slewRatePctPerSec = rate; }

private:
    CanBusManager& _psu;

    // Slew-limited LED current control
    float    _uiSetpointFrac;      // 0..1 from encoder/UI
    float    _appliedCurrentFrac;  // 0..1 actually commanded to PSU
    uint32_t _lastUpdateMs;        // last update time for slew calculations
    float    _slewRatePctPerSec;   // active slew rate (percent per second)
    bool     _isOn;                // true when PSU is enabled
    unsigned long _shutdownStartTimeMs; // tracks when shutdown started for hard timeout

    // Configuration
    static constexpr float MAX_LED_CURRENT_A = 22.0f; // COB nominal

    // Helper: apply a symmetric slew limit toward a target, using the
    // configured percent-per-second rate and elapsed time.
    float applySlew(float currentFrac, float targetFrac, float dtSec) const;
};

