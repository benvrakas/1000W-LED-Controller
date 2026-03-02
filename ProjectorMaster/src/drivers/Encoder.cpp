#include "Encoder.h"

EncoderManager encoder;

EncoderManager::EncoderManager()
    : _counts(0), _lastState(0) {}

void EncoderManager::begin() {
    // Read initial state of the encoder pins
    bool a = (digitalRead(BoardPins::PIN_ENCODER_A) != 0);
    bool b = (digitalRead(BoardPins::PIN_ENCODER_B) != 0);
    _lastState = static_cast<uint8_t>((static_cast<uint8_t>(a) << 1) |
                                      static_cast<uint8_t>(b));
}

void EncoderManager::updateFromPins(bool a, bool b) {
    uint8_t state = static_cast<uint8_t>((static_cast<uint8_t>(a) << 1) |
                                        static_cast<uint8_t>(b));

    // Simple 2-bit Gray code decoder. The valid state transitions for a
    // quadrature encoder form a ring: 00 -> 01 -> 11 -> 10 -> 00.
    int8_t  delta    = 0;
    uint8_t combined = static_cast<uint8_t>((_lastState << 2) | state);

    switch (combined) {
        case 0b0001: // 00 -> 01
        case 0b0111: // 01 -> 11
        case 0b1110: // 11 -> 10
        case 0b1000: // 10 -> 00
            delta = +1;
            break;
        case 0b0010: // 00 -> 10
        case 0b0100: // 01 -> 00
        case 0b1101: // 11 -> 01
        case 0b1011: // 10 -> 11
            delta = -1;
            break;
        default:
            delta = 0; // Invalid or bounce; ignore
            break;
    }

    if (delta != 0) {
        int16_t newCounts = static_cast<int16_t>(_counts + delta);
        if (newCounts < 0) {
            newCounts = 0;
        } else if (newCounts > MAX_COUNTS_RANGE) {
            newCounts = MAX_COUNTS_RANGE;
        }
        _counts = newCounts;
    }

    _lastState = state;
}

void EncoderManager::handleInterruptA() {
    bool a = (digitalRead(BoardPins::PIN_ENCODER_A) != 0);
    bool b = (digitalRead(BoardPins::PIN_ENCODER_B) != 0);
    updateFromPins(a, b);
}

void EncoderManager::handleInterruptB() {
    bool a = (digitalRead(BoardPins::PIN_ENCODER_A) != 0);
    bool b = (digitalRead(BoardPins::PIN_ENCODER_B) != 0);
    updateFromPins(a, b);
}

int16_t EncoderManager::getCounts() const {
    return _counts;
}

void EncoderManager::setCounts(int16_t value) {
    if (value < 0) value = 0;
    if (value > MAX_COUNTS_RANGE) value = MAX_COUNTS_RANGE;
    _counts = value;
}

