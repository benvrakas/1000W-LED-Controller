#pragma once

#include <Arduino.h>

// AnalogPsuBackend
// ----------------
// Drives the UHP-1500-48 (base, non-CAN variant) via its analog PC pin
// (CN71 pin 2, "constant current programming"). The MCU produces a PWM
// signal that an external RC low-pass converts into a DC level.
//
// The PSU's PC pin only behaves deterministically from 20%-100% rated
// current (0.4-4.7 V); below that it's documented as "non-linear". So the
// command fraction is mapped as two distinct zones, not one continuous
// line: a commanded fraction of exactly 0 drives the pin to 0 V (PSU
// current programming off); any fraction above 0, however small, maps
// straight into the 0.4-3.1 V deterministic region (~6.3 A-22.0 A) rather
// than passing through the undefined sub-20% zone.
//
// This backend has no telemetry path. PSU on/off is still controlled by
// PsuService raising the digital PSU_REMOTE pin; this class only sets the
// current setpoint.

class AnalogPsuBackend {
public:
    explicit AnalogPsuBackend(uint8_t pwmPin);

    // Configure the PWM pin and drive it to a safe (zero) level. Safe to
    // call multiple times.
    void begin();

    // Mirror of PsuService's on/off intent. When false, the backend forces
    // PWM duty to zero so the PC pin floats low even if the Remote line
    // briefly stays high during a transition.
    void setEnabled(bool on);

    // 0..1 fraction of MAX_LED_CURRENT_A. Values outside [0, 1] are clamped.
    void setCurrentFraction(float frac);

    // Currently commanded fraction (post-clamp). Used by PsuService to
    // synthesize blind-mode telemetry for the UI.
    float getCurrentFraction() const { return _currentFrac; }

    // Real commanded current in amps for the currently-set fraction (0 A at
    // frac == 0, otherwise the 20%-100%-rated mapping described above).
    // Lets PsuService report telemetry that actually matches what the PC
    // pin is doing, instead of assuming a naive linear frac->amps relation.
    float getCommandedCurrentA() const;

    bool isEnabled() const { return _enabled; }

private:
    uint8_t _pwmPin;
    bool    _enabled;
    float   _currentFrac;

    float fracToCurrentA(float frac) const;
    void  writePwmForFraction(float frac);
};
