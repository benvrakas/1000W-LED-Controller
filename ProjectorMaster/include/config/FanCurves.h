#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// System Configuration: Tachometers & Fan Curves
// ---------------------------------------------------------------------------

namespace TachometerConfig {
    // Deadstart Duty Cycles (minimum PWM to start spinning)
    static constexpr uint8_t  MAIN_PSU_DEADSTART_DUTY = 77;
    static constexpr uint8_t  AUX_DEADSTART_DUTY = 26;
    static constexpr uint8_t  PUMP_DEADSTART_DUTY = 127;

    // RPM Computation Intervals (milliseconds)
    static constexpr unsigned long RPM_COMPUTE_INTERVAL = 200;

    // RPM Limits (for scaling/clamping)
    static constexpr uint16_t MAX_MAIN_PSU_RPM = 3000;
    static constexpr uint16_t MAX_AUX_RPM = 6000;
    static constexpr uint16_t MAX_PUMP_RPM = 4800;

    // Stall Detection Thresholds (RPM below this with duty > 0 = stall)
    static constexpr uint16_t MAIN_PSU_STALL_RPM = 300;
    static constexpr uint16_t AUX_STALL_RPM = 300;
    static constexpr uint16_t PUMP_STALL_RPM = 150;
}

namespace FanCurveConfig {
    // Radiator fans + PSU fan (driven by LED temperature)
    // Quiet (~25%) below 35C, ramping to full by 75C.
    static constexpr float   MAIN_TEMP_MIN = 35.0f;
    static constexpr float   MAIN_TEMP_MAX = 75.0f;
    static constexpr uint8_t MAIN_DUTY_MIN = 64;  // ~25%
    static constexpr uint8_t MAIN_DUTY_MAX = 255; // 100%

    // Pump (driven by water temperature)
    // Baseline ~30% at 30C, ramp to full by 50C.
    static constexpr float   PUMP_TEMP_MIN = 30.0f;
    static constexpr float   PUMP_TEMP_MAX = 50.0f;
    static constexpr uint8_t PUMP_DUTY_MIN = 77;  // ~30%
    static constexpr uint8_t PUMP_DUTY_MAX = 255; // 100%

    // Aux/Lens fan (driven by LED temperature)
    // Baseline ~20% at 30C, ramp to full by 75C.
    static constexpr float   AUX_TEMP_MIN = 30.0f;
    static constexpr float   AUX_TEMP_MAX = 75.0f;
    static constexpr uint8_t AUX_DUTY_MIN = 51;   // ~20%
    static constexpr uint8_t AUX_DUTY_MAX = 255;  // 100%
}
