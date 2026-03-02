#pragma once

#include "util/BoardPins.h"
#include <Arduino.h>

// EncoderManager
// --------------
// Simple quadrature encoder interface for the front-panel knob.
//
// Hardware:
//  - 600 pulses per revolution, wired to RX/TX (D14/D15).
//  - Two full revolutions span 0–100% LED power, i.e. 0–1200 logical counts.

class EncoderManager {
public:
    EncoderManager();

    // Initialize internal state. Call after pins are configured.
    void begin();

    // ISR entry points, called from the global encoderAISR/encoderBISR
    // bridges when either channel changes.
    void handleInterruptA();
    void handleInterruptB();

    // Get or set the current encoder count (0..MAX_COUNTS_RANGE).
    int16_t getCounts() const;
    void    setCounts(int16_t value);

    static constexpr int16_t COUNTS_PER_REV   = 600;
    static constexpr int16_t MAX_COUNTS_RANGE = 2 * COUNTS_PER_REV; // 0..1200

private:
    volatile int16_t _counts;
    volatile uint8_t _lastState; // 2-bit state of (A,B)

    void updateFromPins(bool a, bool b);
};

// Global encoder manager instance
extern EncoderManager encoder;

// ISR bridge function declarations (implemented in HardwareBridges.cpp)
void encoderAISR();
void encoderBISR();
