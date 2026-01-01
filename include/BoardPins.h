#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 

/**
 * 1000W LED CONTROLLER - CARRIER BOARD PIN MAPPINGS
 * Using static constexpr int for better type safety and debugging.
 */

// ---Encoder Sensors---
static constexpr int PIN_ENCODER_A = A5;    // Encoder A (Pull-up)
static constexpr int PIN_ENCODER_B = A4;    // Encoder B (Pull-up)

// ---Power Switch Setup---
static constexpr int PIN_SW_BTN = A3;       // Switch (Pull-up)
static constexpr int PIN_SW_LED = A2;       // Illuminated Switch LED Ring

// --- THERMAL SENSORS ---
static constexpr int PIN_THERM_LED    = A0;  // LED Temp
static constexpr int PIN_THERM_WATER  = A1;  // Water Temp

// --- ISOLATED PWM OUTPUTS (Via ADuM) ---
static constexpr int PIN_RAD_FANS_PWM  = 0;  // Radiator Fan Speed Control
static constexpr int PIN_PUMP_PWM = 4;       // Water Pump Speed Control
static constexpr int PIN_PSU_FAN_PWM  = 2;      // PSU Fan Speed Control (SCK)
static constexpr int PIN_AUX_FAN_PWM  = 6;   // D6: Onboard MOSFET (2-wire Aux)

// --- ISOLATED TACH INPUTS (Via ADuM) ---
static constexpr int PIN_RAD_FAN_TACH  = 1;    // Radiator Fan RPM Feedback
static constexpr int PIN_PUMP_TACH = 5;    // Water Pump RPM Feedback
static constexpr int PIN_PSU_FAN_TACH  = 3;    // PSU Fan RPM Feedback (MOSI)

/**
 * Helper function to initialize all carrier board pins.
 * Ensures the 1000W system starts in a safe (OFF) state.
 */
inline void initBoardPins() {
    // Encoder Sensor Setup
    pinMode(PIN_ENCODER_A, INPUT);
    pinMode(PIN_ENCODER_B, INPUT);

    // Power Button Setup
    pinMode(PIN_SW_BTN, INPUT_PULLUP);
    pinMode(PIN_SW_LED, OUTPUT);
    digitalWrite(PIN_SW_LED, LOW); // Start with LED off

    // Thermal Sensor Setup
    pinMode(PIN_THERM_LED, INPUT);
    pinMode(PIN_THERM_WATER, INPUT);

    // PWM Outputs - Force LOW immediately for safety
    pinMode(PIN_RAD_FANS_PWM, OUTPUT);
    digitalWrite(PIN_RAD_FANS_PWM, LOW);
    
    pinMode(PIN_PUMP_PWM, OUTPUT);
    digitalWrite(PIN_PUMP_PWM, LOW);
    
    pinMode(PIN_PSU_FAN_PWM, OUTPUT);
    digitalWrite(PIN_PSU_FAN_PWM, LOW);

    pinMode(PIN_AUX_FAN_PWM, OUTPUT);
    digitalWrite(PIN_AUX_FAN_PWM, LOW);

    // Tachometer Inputs
    pinMode(PIN_RAD_FAN_TACH, INPUT);
    pinMode(PIN_PUMP_TACH, INPUT);
    pinMode(PIN_PSU_FAN_TACH, INPUT);

    //OLED Configuration
    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

    //OLED Display Setup (this should maybe go else where, maybe in the syteminit)
    /*
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED failed"));
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(25, 8);
    display.print("V-LABS");
    display.display();
    */
}

#endif // BOARD_PINS_H