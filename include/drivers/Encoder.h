#pragma once

#include "config/PinMap.h"
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

    // Atomically read and clear the count of illegal (double-bit) quadrature
    // transitions observed since the last call. A sustained burst indicates
    // a failing or disconnected encoder channel; occasional single events
    // can occur since the hardware glitch filter is disabled on these EIC
    // lines (see SystemStartup::isrInit) to avoid dropping real pulses at
    // high spin rates.
    uint16_t consumeIllegalTransitionCount();

    static constexpr int16_t COUNTS_PER_REV   = 600;
    static constexpr float   REVS_FOR_MAX_BRIGHTNESS = 2.0f; // Adjustable constant for revolutions mapped to 0-1 brightness
    static constexpr int16_t MAX_COUNTS_RANGE = static_cast<int16_t>(REVS_FOR_MAX_BRIGHTNESS * COUNTS_PER_REV); // e.g. 1200


private:
    volatile int16_t _counts;
    volatile uint8_t _lastState; // 2-bit state of (A,B)
    volatile uint16_t _illegalTransitions;

    void updateFromPins(bool a, bool b);
};

// Global encoder manager instance
extern EncoderManager encoder;

// ISR bridge function declarations (implemented in HardwareBridges.cpp)
void encoderAISR();
void encoderBISR();
