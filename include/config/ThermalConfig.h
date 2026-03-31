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

    static constexpr float NOMINAL_RESISTANCE_LED = 10000.0f; // 10K NTC
    static constexpr float NOMINAL_RESISTANCE_PUMP = 10000.0f; // 10K NTC

    static constexpr uint32_t SERIES_RESISTOR_LED = 10000UL; // 10K Fixed
    static constexpr uint32_t SERIES_RESISTOR_PUMP = 10000UL; // 10K Fixed

    // Hardware Orientation:
    // true  = 3.3V -- [Thermistor] -- PIN -- [Resistor] -- GND
    // false = 3.3V -- [Resistor] -- PIN -- [Thermistor] -- GND
    static constexpr bool IS_HIGH_SIDE_LED = false; 
    static constexpr bool IS_HIGH_SIDE_PUMP = false;
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
    // Limits
    static constexpr uint8_t MAIN_DUTY_MIN = 64;  // ~25%
    static constexpr uint8_t MAIN_DUTY_MAX = 255; // 100%

    static constexpr uint8_t PUMP_DUTY_MIN = 77;  // ~30%
    static constexpr uint8_t PUMP_DUTY_MAX = 255; // 100%

    static constexpr uint8_t AUX_DUTY_MIN = 51;   // ~20%
    static constexpr uint8_t AUX_DUTY_MAX = 255;  // 100%

    // PI Controller Configuration (Target Temps & Tuning)
    // LED Cooling (Radiator, PSU, Aux Fans)
    static constexpr float TARGET_TEMP_LED = 55.0f;
    static constexpr float LED_KP = 20.0f;  // Duty increase per degree C above target
    static constexpr float LED_KI = 0.5f;   // Duty increase per degree C per second
    static constexpr float LED_INTEGRAL_MAX = 100.0f; // Limit integral windup

    // Water Cooling (Pump)
    static constexpr float TARGET_TEMP_WATER = 40.0f;
    static constexpr float WATER_KP = 15.0f;
    static constexpr float WATER_KI = 0.3f;
    static constexpr float WATER_INTEGRAL_MAX = 100.0f;
}
