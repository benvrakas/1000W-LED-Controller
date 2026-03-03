#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Thermal Subsystem Configuration
// Includes: Thermistors, Tachometers (Fans/Pump), and Cooling Curves
// ---------------------------------------------------------------------------

namespace ThermistorConfig {
    static constexpr float MAX_TEMP_LED = 75.0f;
    static constexpr float MAX_TEMP_PUMP = 50.0f;

    static constexpr float BETA_VALUE_LED = 3950.0f;
    static constexpr float BETA_VALUE_PUMP = 3950.0f;

    static constexpr uint32_t SERIES_RESISTOR_LED = 10000UL; // 10k Ohm
    static constexpr uint32_t SERIES_RESISTOR_PUMP = 10000UL; // 12-bit ADC
}

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
    static constexpr float   MAIN_TEMP_MIN = 35.0f;
    static constexpr float   MAIN_TEMP_MAX = 75.0f;
    static constexpr uint8_t MAIN_DUTY_MIN = 64;  // ~25%
    static constexpr uint8_t MAIN_DUTY_MAX = 255; // 100%

    // Pump (driven by water temperature)
    static constexpr float   PUMP_TEMP_MIN = 30.0f;
    static constexpr float   PUMP_TEMP_MAX = 50.0f;
    static constexpr uint8_t PUMP_DUTY_MIN = 77;  // ~30%
    static constexpr uint8_t PUMP_DUTY_MAX = 255; // 100%

    // Aux/Lens fan (driven by LED temperature)
    static constexpr float   AUX_TEMP_MIN = 30.0f;
    static constexpr float   AUX_TEMP_MAX = 75.0f;
    static constexpr uint8_t AUX_DUTY_MIN = 51;   // ~20%
    static constexpr uint8_t AUX_DUTY_MAX = 255;  // 100%
}
