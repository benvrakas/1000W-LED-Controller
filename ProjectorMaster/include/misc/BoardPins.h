#pragma once

#include <Arduino.h>

namespace BoardPins {
    // ---Encoder Sensors---
    // New hardware mapping: Encoder A/B moved to RX/TX (D14/D15)
    static constexpr uint8_t PIN_ENCODER_A = 14;    // RX: Encoder A (Pull-up)
    static constexpr uint8_t PIN_ENCODER_B = 15;    // TX: Encoder B (Pull-up)

    // ---Power Switch Setup---
    static constexpr uint8_t PIN_SW_BTN = A3;       // Switch (Pull-up)
    static constexpr uint8_t PIN_SW_LED = A2;       // Illuminated Switch LED Ring

    // ---THERMAL SENSORS---
    // New mapping: LED and Water thermistors on A0/A1
    static constexpr uint8_t PIN_THERM_LED = A0;     // LED Temp
    static constexpr uint8_t PIN_THERM_WATER = A1;   // Water Temp

    // ---ISOLATED PWM OUTPUTS (Via ADuM)---
    // New mapping from architecture v2.0 notes
    static constexpr uint8_t PIN_RAD_FANS_PWM = 0;   // D0: Radiator Fan Speed Control (fans 1–3)
    static constexpr uint8_t PIN_PUMP_PWM     = 4;   // D4: Water Pump Speed Control
    static constexpr uint8_t PIN_PSU_FAN_PWM  = 2;   // D2: PSU Fan Speed Control
    static constexpr uint8_t PIN_AUX_FAN_PWM  = 6;   // D6: Aux/Lens Cooling Fan Speed Control

    // ---ISOLATED TACH INPUTS (Via ADuM)---
    // New mapping from architecture v2.0 notes
    static constexpr uint8_t PIN_RAD_FAN_TACH = 1;   // D1: Radiator Fan RPM Feedback
    static constexpr uint8_t PIN_PUMP_TACH    = 5;   // D5: Water Pump RPM Feedback
    static constexpr uint8_t PIN_PSU_FAN_TACH = 3;   // D3: PSU Fan RPM Feedback
    static constexpr uint8_t PIN_AUX_FAN_TACH = 16;  // D16/SPARE: Aux/Lens Cooling Fan RPM Feedback

    // ---CAN + OLED I2C Pins---
    // CAN transceiver (ISO1050) lives on the original SDA/SCL pins
    static constexpr uint8_t PIN_CAN_TX   = SDA; // D11
    static constexpr uint8_t PIN_CAN_RX   = SCL; // D12

    // OLED now moved to a secondary I2C bus on A4/A5 (SERCOM5, PIO_SERCOM_ALT)
    static constexpr uint8_t PIN_OLED_SDA = A4;
    static constexpr uint8_t PIN_OLED_SCL = A5;

    // ---PSU CONTROL LINES---
    static constexpr uint8_t PIN_PSU_ENABLE = 7;  // D7: Existing hardware enable line to PSU logic
    static constexpr uint8_t PIN_PSU_REMOTE = SCK; // SCK pin: drives N-MOSFET gating UHP-1500 remote on/off

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
