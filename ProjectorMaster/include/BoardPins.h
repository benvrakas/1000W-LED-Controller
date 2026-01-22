#pragma once

#include <Arduino.h>

namespace BoardPins {
    // ---Encoder Sensors---
    static constexpr uint8_t PIN_ENCODER_A = A5;    // Encoder A (Pull-up)
    static constexpr uint8_t PIN_ENCODER_B = A4;    // Encoder B (Pull-up)

    // ---Power Switch Setup---
    static constexpr uint8_t PIN_SW_BTN = A3;       // Switch (Pull-up)
    static constexpr uint8_t PIN_SW_LED = A2;       // Illuminated Switch LED Ring

    // ---THERMAL SENSORS---
    static constexpr uint8_t PIN_THERM_LED = A0;  // LED Temp
    static constexpr uint8_t PIN_THERM_WATER = A1;  // Water Temp

    // ---ISOLATED PWM OUTPUTS (Via ADuM)---
    static constexpr uint8_t PIN_RAD_FANS_PWM = 0;  // Radiator Fan Speed Control
    static constexpr uint8_t PIN_PUMP_PWM = 4;       // Water Pump Speed Control
    static constexpr uint8_t PIN_PSU_FAN_PWM = 2;      // PSU Fan Speed Control (SCK)
    static constexpr uint8_t PIN_AUX_FAN_PWM = 6;   // D6: Onboard MOSFET (2-wire Aux)

    // ---ISOLATED TACH INPUTS (Via ADuM)---
    static constexpr uint8_t PIN_RAD_FAN_TACH = 1;    // Radiator Fan RPM Feedback
    static constexpr uint8_t PIN_PUMP_TACH = 5;    // Water Pump RPM Feedback
    static constexpr uint8_t PIN_PSU_FAN_TACH = 3;    // PSU Fan RPM Feedback (MOSI)
    static constexpr uint8_t PIN_AUX_FAN_TACH = 13;

    // ---I2C PIN DECLERATION---
    static constexpr uint8_t PIN_I2C_SDA = SDA; 
    static constexpr uint8_t PIN_I2C_SCL = SCL;
}
