#include "drivers/AnalogPsuBackend.h"

#include "config/PowerConfig.h"

AnalogPsuBackend::AnalogPsuBackend(uint8_t pwmPin)
    : _pwmPin(pwmPin), _enabled(false), _currentFrac(0.0f) {}

void AnalogPsuBackend::begin() {
    pinMode(_pwmPin, OUTPUT);
    analogWrite(_pwmPin, 0);
    _enabled     = false;
    _currentFrac = 0.0f;
}

void AnalogPsuBackend::setEnabled(bool on) {
    _enabled = on;
    if (!on) {
        analogWrite(_pwmPin, 0);
    } else {
        writePwmForFraction(_currentFrac);
    }
}

void AnalogPsuBackend::setCurrentFraction(float frac) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    _currentFrac = frac;
    if (_enabled) writePwmForFraction(frac);
}

float AnalogPsuBackend::getCommandedCurrentA() const {
    return fracToCurrentA(_currentFrac);
}

float AnalogPsuBackend::fracToCurrentA(float frac) const {
    // 0% command = PSU current programming off (0 A) -- PsuService is
    // responsible for never sending this while armed (see its update()),
    // since a 0V PC pin appears to leave the PSU in an unregulated fail-open
    // mode rather than actually going dim. Any command above 0% maps into
    // the PSU's deterministic linear region, floored at PC_VOLTAGE_MIN_ARMED
    // (not the datasheet's bare 20%-rated minimum) for margin -- there's no
    // representable command between 0 A and that floor, so we jump straight
    // to it rather than driving the PC pin toward the undefined sub-20% zone.
    if (frac <= 0.0f) return 0.0f;

    const float maxLedA = PsuConfig::MAX_LED_CURRENT_A;
    const float ratedA  = AnalogPsuConfig::PSU_RATED_CURRENT_A;
    const float v20     = AnalogPsuConfig::PC_VOLTAGE_AT_20PCT;
    const float v100    = AnalogPsuConfig::PC_VOLTAGE_AT_100PCT;
    const float vFloor  = AnalogPsuConfig::PC_VOLTAGE_MIN_ARMED;

    // Same voltage<->rated-fraction line as writePwmForFraction() below,
    // evaluated at the floor voltage, so the current this reports matches
    // what's actually commanded on the pin at minimum.
    const float floorRatedFrac = 0.2f + (vFloor - v20) * 0.8f / (v100 - v20);
    const float floorA = floorRatedFrac * ratedA;

    return floorA + frac * (maxLedA - floorA);
}

void AnalogPsuBackend::writePwmForFraction(float frac) {
    if (frac <= 0.0f) {
        analogWrite(_pwmPin, 0);
        return;
    }

    // Map commanded current (see fracToCurrentA) to a PC voltage using the
    // datasheet's linear region: V_PC = V20 + (I/Irated - 0.2) * (V100 - V20) / 0.8
    const float maxLedA = PsuConfig::MAX_LED_CURRENT_A;
    const float ratedA  = AnalogPsuConfig::PSU_RATED_CURRENT_A;
    const float v20     = AnalogPsuConfig::PC_VOLTAGE_AT_20PCT;
    const float v100    = AnalogPsuConfig::PC_VOLTAGE_AT_100PCT;
    const float vRail   = AnalogPsuConfig::MCU_PWM_RAIL_V;

    float targetA   = fracToCurrentA(frac);
    float ratedFrac = targetA / ratedA;
    float pcVoltage = v20 + (ratedFrac - 0.2f) * (v100 - v20) / 0.8f;
    if (pcVoltage < v20) pcVoltage = v20;     // guard fp rounding just under the floor
    if (pcVoltage > vRail) pcVoltage = vRail; // can't exceed MCU rail

    float dutyNorm = pcVoltage / vRail;
    int   duty8    = static_cast<int>(dutyNorm * 255.0f + 0.5f);

    // Ceiling derived from MAX_LED_CURRENT_A so 100% UI = exactly 22 A
    float maxRatedFrac = maxLedA / ratedA;
    float maxPcVoltage = v20 + (maxRatedFrac - 0.2f) * (v100 - v20) / 0.8f;
    int   dutyCeil8    = static_cast<int>((maxPcVoltage / vRail) * 255.0f + 0.5f);
    if (dutyCeil8 > 255) dutyCeil8 = 255;

    if (duty8 < AnalogPsuConfig::PWM_DUTY_FLOOR_8BIT) duty8 = AnalogPsuConfig::PWM_DUTY_FLOOR_8BIT;
    if (duty8 > dutyCeil8) duty8 = dutyCeil8;

    analogWrite(_pwmPin, static_cast<uint8_t>(duty8));
}
