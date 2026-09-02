#pragma once

#include <Arduino.h>

namespace PinMap {
    // ---Encoder Sensors---
    // Optimized for Feather M4 CAN Express
    static constexpr uint8_t PIN_ENCODER_A = 0;     // RX (PB17): Encoder A (EIC 1)
    static constexpr uint8_t PIN_ENCODER_B = 1;     // TX (PB16): Encoder B (EIC 0)

    // ---Power Switch Setup---
    static constexpr uint8_t PIN_SW_BTN = A3;       // A3 (PB09): Switch Signal
    static constexpr uint8_t PIN_SW_LED = A2;       // A2 (PB08): Switch LED

    // ---THERMAL SENSORS---
    static constexpr uint8_t PIN_THERM_LED = A0;     // A0 (PA02): 10K+10K voltage divider for LED Temp
    static constexpr uint8_t PIN_THERM_WATER = A1;   // A1 (PA05): 10K+10K voltage divider for Water Temp

    // ---PWM OUTPUTS---
    static constexpr uint8_t PIN_RAD_FANS_PWM = 5;   // D5 (PA16): Radiator Fan PWM
    static constexpr uint8_t PIN_PUMP_PWM     = 6;   // D6 (PA18): Pump PWM
    static constexpr uint8_t PIN_PSU_FAN_PWM  = 9;   // D9 (PA19): PSU Fan PWM
    static constexpr uint8_t PIN_AUX_FAN_PWM  = 4;   // D4 (PA14): Aux Fan PWM

    // ---TACH INPUTS---
    static constexpr uint8_t PIN_RAD_FAN_TACH = 10;  // D10 (PA20): Radiator Fan Tach (EIC 4)
    static constexpr uint8_t PIN_PUMP_TACH    = 11;  // D11 (PA21): Pump Tach (EIC 5)
    static constexpr uint8_t PIN_PSU_FAN_TACH = 12;  // D12 (PA22): PSU Fan Tach (EIC 6)
    static constexpr uint8_t PIN_AUX_FAN_TACH = 13;  // D13 (PA23): Aux Fan Tach (EIC 7)

    // ---CAN BUS---
    // Feather M4 CAN has built-in CAN transceiver terminal block
    // No GPIO pins needed to be passed around

    // ---OLED Display---
    // Standard I2C on SDA/SCL (Feather Wing OLED)
    static constexpr uint8_t PIN_OLED_SDA = SDA;     // SDA (PA12)
    static constexpr uint8_t PIN_OLED_SCL = SCL;     // SCL (PA13)

    // ---PSU CONTROL LINES---
    static constexpr uint8_t PIN_PSU_REMOTE = A4;    // A4 (PA04): Remote On/Off
    static constexpr uint8_t PIN_PSU_PC_PWM = A5;    // A5 (PA06): Analog current programming
                                                     // PWM -> external RC -> CN71 pin 2 (PC).
                                                     // Only used when PsuControlConfig::PSU_CONTROL_VIA_CAN == false.

    // ---Status NeoPixel---
    // Renamed to avoid collision with board macros like PIN_NEOPIXEL or NEOPIXEL
    static constexpr uint8_t PIN_STATUS_LED = 8; 

    // ---EIC Channels---
    // Mapped based on SAMD51 Datasheet/Variant for Feather M4 CAN Express
    // Note: Verify variant.cpp for exact EIC mappings on your board version
    static constexpr IRQn_Type EIC_CHANNEL_BUTTON = EIC_9_IRQn;         // A3 (PB09) -> EIC 9
    static constexpr IRQn_Type EIC_CHANNEL_ENCODER_A = EIC_1_IRQn;      // RX (PB17) -> EIC 1
    static constexpr IRQn_Type EIC_CHANNEL_ENCODER_B = EIC_0_IRQn;      // TX (PB16) -> EIC 0
    static constexpr IRQn_Type EIC_CHANNEL_MAIN_FAN_TACH = EIC_4_IRQn;  // D10 (PA20) -> EIC 4
    static constexpr IRQn_Type EIC_CHANNEL_PUMP_TACH = EIC_5_IRQn;      // D11 (PA21) -> EIC 5
    static constexpr IRQn_Type EIC_CHANNEL_PSU_FAN_TACH = EIC_6_IRQn;   // D12 (PA22) -> EIC 6
    static constexpr IRQn_Type EIC_CHANNEL_AUX_FAN_TACH = EIC_7_IRQn;   // D13 (PA23) -> EIC 7

    // ---EIC Priority Ranks---
    static constexpr uint8_t EIC_PRIORITY_BUTTON = 0;
    static constexpr uint8_t EIC_PRIORITY_TACH = 1;
    static constexpr uint8_t EIC_PRIORITY_ENCODER = 2;
}
