#pragma once

#include <Arduino.h>

// CoolingState
// ------------
// Read-only snapshot of current cooling system telemetry, returned by
// CoolingService::getState() for use by UI and logging systems.

struct CoolingState {
    float    ledTempC;
    float    waterTempC;
    uint16_t mainFanRPM;
    uint16_t auxFanRPM;
    uint16_t psuFanRPM;
    uint16_t pumpRPM;
    uint8_t  mainFanDuty;
    uint8_t  auxFanDuty;
    uint8_t  psuFanDuty;
    uint8_t  pumpDuty;
};

// Forward declaration
class TachometerManager;
class ThermistorManager;
struct SystemController;

// CoolingService
// ---------------
// Mid-level policy layer for all active cooling devices (radiator fans,
// PSU fan, pump, and aux/lens fan). This class is responsible for deciding
// *what* speeds the cooling channels should run at, based on system
// telemetry and error conditions, while the low-level TachometerManager
// handles *how* those speeds are driven in hardware.

class CoolingService {
public:
    CoolingService(TachometerManager& mainFan, TachometerManager& psuFan,
                   TachometerManager& pump, TachometerManager& auxFan,
                   ThermistorManager& ledThermistor, ThermistorManager& pumpThermistor);

    // Initialize any internal state. Called once when entering RUN.
    void begin();

    // Periodic update invoked from the RUN state. Reads temperatures,
    // applies cooling policy, and drives PWM outputs. ledDutyFraction is the
    // LED's actual applied current fraction (0..1, PsuService::
    // getAppliedCurrentFraction()) -- nonzero ("LED on") overrides two
    // channels' normal thermal-PI control entirely:
    //  - pump goes to full speed, since it must run flat out any time the
    //    LED is actively drawing power, not just once the coolant is warm.
    //  - PSU fan follows a fixed linear rule (20% of LED duty) instead of
    //    the LED-temp PI curve main/aux still use.
    // Both fall back to their prior PI-based behavior once the LED is off.
    void update(unsigned long now, float ledDutyFraction);

    // Get the latest cooling state (temperatures + RPMs)
    const CoolingState& getState() const { return _state; }

private:
    TachometerManager& _mainFan;
    TachometerManager& _psuFan;
    TachometerManager& _pump;
    TachometerManager& _auxFan;
    ThermistorManager& _ledThermistor;
    ThermistorManager& _pumpThermistor;

    CoolingState _state;

    // PI Controller State -- water-temp loop only now; the pump's LED-temp
    // PI fallback was removed in favor of a fixed idle duty (see .cpp).
    unsigned long _lastUpdateMs;
    float _waterIntegral;
};
