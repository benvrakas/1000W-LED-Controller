#pragma once

#include <Arduino.h>

#include "drivers/Encoder.h"
#include "drivers/PowerButton.h"

// InputService
// ------------
// Black-box abstraction over human inputs (power button + encoder).
//
// Responsibilities:
//  - Enforce 3 s hold-to-arm semantics via PowerButtonManager.
//  - Expose stable "armed" state and ON/OFF edges.
//  - Provide a 0..1 knob fraction derived from encoder counts.
//
// This isolates higher-level code (RUN state, PSU service, UI) from
// direct use of encoder/power button drivers.

class InputService {
public:
    InputService();

    void begin();
    void update(unsigned long now);

    bool  isArmed() const { return _armed; }
    bool  edgeArmedOn() const { return _edgeArmedOn; }
    bool  edgeArmedOff() const { return _edgeArmedOff; }

    // 0..1 representation of encoder position (0..MAX_COUNTS_RANGE)
    float getKnobFraction() const { return _knobFraction; }

    // Force the knob representation back to 0. Intended for use when the
    // system is transitioning to an OFF/shutdown state so that higher-level
    // logic does not need to talk to the encoder driver directly.
    void forceKnobToZero();

    // Update the power button LED state (encapsulates the GPIO)
    void setButtonLed(bool on);

private:
    bool  _armed;
    bool  _prevArmed;
    bool  _edgeArmedOn;
    bool  _edgeArmedOff;
    float _knobFraction;
};

