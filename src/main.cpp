/**
 * Project: 1000W LED V-LABS Controller
 * Hardware: Adafruit Feather M4 Express + FeatherWing OLED (128x32)
 * PSU: Mean Well UHP-1500-48
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//---SYSTEM STATE---
bool isSystemOn = false;
unsigned long buttonPressStartTime = 0;
bool lastButtonState = HIGH; // Pull-up means HIGH is unpressed
const unsigned long POWER_ON_THRESHOLD = 3000; // 3 seconds

//---ENCODER STATE---

// --- OLED CONFIGURATION ---
// FeatherWing OLED is typically 128x32
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void updateDisplay(const char* status, const char* msg) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(status);
    display.setTextSize(2);
    display.println(msg);
    display.display();
}

//---Read Encoder Function---
//IRAM_ATTR forces compiler to place this code in ram
/*
void IRAM_ATTR readEncoder() {
    int aState = digitalRead(PIN_ENC_A);
    int bState = digitalRead(PIN_ENC_B);

    if (aState == bState) {
        encoderPosition--; // Clockwise or Counter-Clockwise depending on mounting
    } else {
        encoderPosition++;
    }
}
*/

/**
 * Forces the system into a safe, locked state immediately.
 * Resets variables and turns off the physical indicator.
 */
void killPower() {
    isSystemOn = false;
    digitalWrite(PIN_SW_LED, LOW); // Turn off the button ring LED
    updateDisplay("SYSTEM:", "OFF");
    Serial.println("SHUTDOWN: System killed instantly.");
}

/**
 * Activates the system state after the 3s threshold is met.
 */
void powerOn() {
    isSystemOn = true;
    digitalWrite(PIN_SW_LED, HIGH); // Turn on the button ring LED
    updateDisplay("SYSTEM:", "ACTIVE");
    Serial.println(">>> EVENT: POWER ON (3s Hold Successful)");
}

// --- PWM SETTINGS ---
const int PWM_RES = 8;      // 0-255 range
const int INITIAL_DUTY_1_PCT = 3; // ~1% of 255

void setup() {
    Serial.begin(115200);

    // 1. PIN MODES
    pinMode(PIN_SW_BTN, INPUT_PULLUP);
    pinMode(PIN_ENCODER_A,  INPUT_PULLUP);
    pinMode(PIN_ENCODER_B,  INPUT_PULLUP);
    pinMode(PIN_SW_LED, OUTPUT);
    digitalWrite(PIN_SW_LED, LOW);

    // 2. OLED INITIALIZATION
    // Address 0x3C is standard for these wings
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED failed"));
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(25, 8);
    display.print("V-LABS");
    display.display();

     //Initialize Pins
    pinMode(PIN_SW_BTN, INPUT_PULLUP);
    pinMode(PIN_SW_LED, OUTPUT);
    digitalWrite(PIN_SW_LED, LOW); // Start with LED off

    Serial.println("Button Test Initialized.");
    Serial.println("Hold button for 3 seconds to Power On.");

    // 3. PWM SETUP & 1% INITIALIZATION
    analogWriteResolution(PWM_RES);
    
    // Set all fan/pump pins as outputs
    pinMode(PIN_RAD_FANS_PWM, OUTPUT);
    pinMode(PIN_PSU_FAN_PWM,  OUTPUT);
    pinMode(PIN_PUMP_PWM,     OUTPUT);
    pinMode(PIN_AUX_FAN_PWM,  OUTPUT);
    
    // Command 1% Duty Cycle (Step B in flow chart)
    analogWrite(PIN_RAD_FANS_PWM, INITIAL_DUTY_1_PCT);
    analogWrite(PIN_PSU_FAN_PWM,  INITIAL_DUTY_1_PCT);
    analogWrite(PIN_PUMP_PWM,     INITIAL_DUTY_1_PCT);
    analogWrite(PIN_AUX_FAN_PWM,  INITIAL_DUTY_1_PCT);

    delay(2000); // 2s Boot Grace Period from Eraser.io chart

    // Update Display to show status
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("System: TEST MODE");
    display.println("Fans: 1% Duty Cycle");
    display.display();
}

void loop() {
    // Main loop logic will go here
     bool currentButtonState = digitalRead(PIN_SW_BTN);

     // --- BUTTON PRESSED (Logic LOW) ---
    if (currentButtonState == LOW) {
        
        if (lastButtonState == HIGH) {
            // Moment of initial press
            buttonPressStartTime = millis();
            Serial.println("Button Pressed...");
        }

        if (!isSystemOn) {
            // While holding to turn ON
            unsigned long heldDuration = millis() - buttonPressStartTime;
            
            // Visual feedback while holding (blink LED or show progress)
            if (heldDuration > 500) {
                display.clearDisplay();
                display.setCursor(0,0);
                display.setTextSize(1);
                display.print("HOLDING: ");
                display.print(heldDuration / 1000.0);
                display.println("s");
                display.drawRect(0, 20, map(heldDuration, 0, POWER_ON_THRESHOLD, 0, 128), 8, SSD1306_WHITE);
                display.display();
            }

            if (heldDuration >= POWER_ON_THRESHOLD) {
                isSystemOn = true;
                digitalWrite(PIN_SW_LED, HIGH); // Light up the button
                updateDisplay("SYSTEM:", "ACTIVE");
                Serial.println("POWER ON - Threshold reached.");
            }
        }
    } 
    // --- BUTTON RELEASED (Logic HIGH) ---
    else {
        if (lastButtonState == LOW) {
            // Moment of release
            Serial.println("Button Released.");
            
            if (isSystemOn) {
                // INSTANT OFF LOGIC
                // If it was already on, any press-and-release (or just press) triggers off
                // We handle it on release here to differentiate from the initial hold-to-on
                isSystemOn = false;
                digitalWrite(PIN_SW_LED, LOW);
                updateDisplay("SYSTEM:", "OFF");
                Serial.println("POWER OFF - Instant trigger.");
            } else {
                // If they let go before 3 seconds
                updateDisplay("SYSTEM:", "LOCKED");
            }
        }
    }

    lastButtonState = currentButtonState;
    delay(10); // Small debounce/stability delay
    
    //Test our encoder reading function and print results to console
    // Check if position changed
    /*
    if (encoderPosition != lastReportedPos) {
        lastReportedPos = encoderPosition;

        // Print to Serial Monitor
        Serial.print("Encoder Count: ");
        Serial.print(lastReportedPos);
        Serial.print(" | Setpoint: ");
        // Example: Map raw count to 0-1000W for visualization
        Serial.print(constrain(lastReportedPos, 0, 1000)); 
        Serial.println(" W");

        // Update OLED
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("Setpoint: ");
        display.print(lastReportedPos);
        
        display.drawRect(0, 15, map(constrain(lastReportedPos, 0, 1000), 0, 1000, 0, 128), 10, SSD1306_WHITE);
        display.display();
    */
  
}