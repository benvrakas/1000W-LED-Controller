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

    // ---EIC Channels---
    static constexpr IRQn_Type EIC_CHANNEL_BUTTON = EIC_9_IRQn;
    static constexpr IRQn_Type EIC_CHANNEL_PUMP_TACH = EIC_0_IRQn;
    static constexpr IRQn_Type EIC_CHANNEL_MAIN_FAN_TACH = EIC_1_IRQn;
    static constexpr IRQn_Type EIC_CHANNEL_AUX_FAN_TACH = EIC_0_IRQn;
    static constexpr IRQn_Type EIC_CHANNEL_PSU_FAN_TACH = EIC_7_IRQn;
    static constexpr IRQn_Type EIC_CHANNEL_ENCODER_A = EIC_6_IRQn;
    static constexpr IRQn_Type EIC_CHANNEL_ENCODER_B = EIC_4_IRQn;

    // ---EIC Priority Ranks---
    static constexpr uint8_t EIC_PRIORITY_BUTTON = 0;
    static constexpr uint8_t EIC_PRIORITY_TACH = 1;
    static constexpr uint8_t EIC_PRIORITY_ENCODER = 2;
    
}
