#include "Encoder.h"

EncoderManager encoder;

EncoderManager::EncoderManager()
    : _counts(0), _lastState(0), _illegalTransitions(0) {}

void EncoderManager::begin() {
    // Port B (Group 1): PB17=A, PB16=B
    uint32_t in = PORT->Group[1].IN.reg;
    bool a = (in & (1ul << 17)) != 0;
    bool b = (in & (1ul << 16)) != 0;
    _lastState = static_cast<uint8_t>((static_cast<uint8_t>(a) << 1) |
                                      static_cast<uint8_t>(b));
}

// Ultra-fast ISR handlers using register reads and inlined logic
void EncoderManager::handleInterruptA() {
    uint32_t in = PORT->Group[1].IN.reg;
    uint8_t state = (((in >> 17) & 1) << 1) | ((in >> 16) & 1);
    
    if (state == _lastState) return;

    // Table-based quadrature decoding (Direction reversed for user PCB)
    // 00->01: +1 | 01->11: +1 | 11->10: +1 | 10->00: +1
    // (Actual direction reversed to -1 below)
    uint8_t combined = (_lastState << 2) | state;
    _lastState = state;

    int8_t delta = 0;
    switch (combined) {
        case 0b0001: case 0b0111: case 0b1110: case 0b1000: delta = -1; break;
        case 0b0010: case 0b1011: case 0b1101: case 0b0100: delta = +1; break;
    }

    if (delta != 0) {
        int16_t next = _counts + delta;
        if (next < 0) next = 0;
        else if (next > MAX_COUNTS_RANGE) next = MAX_COUNTS_RANGE;
        _counts = next;
    } else {
        // combined matched neither valid single-step case (state != _lastState
        // was already confirmed above), so this is one of the 4 illegal
        // double-bit transitions (both channels appeared to change at once).
        _illegalTransitions++;
    }
}

void EncoderManager::handleInterruptB() {
    handleInterruptA(); // Logic is symmetric for both pins
}

int16_t EncoderManager::getCounts() const {
    // Atomic read for 16-bit volatile variable on 32-bit SAMD51 is generally safe,
    // but noInterrupts() ensures absolute consistency.
    noInterrupts();
    int16_t c = _counts;
    interrupts();
    return c;
}

void EncoderManager::setCounts(int16_t value) {
    if (value < 0) value = 0;
    if (value > MAX_COUNTS_RANGE) value = MAX_COUNTS_RANGE;
    noInterrupts();
    _counts = value;
    interrupts();
}

uint16_t EncoderManager::consumeIllegalTransitionCount() {
    noInterrupts();
    uint16_t count = _illegalTransitions;
    _illegalTransitions = 0;
    interrupts();
    return count;
}
