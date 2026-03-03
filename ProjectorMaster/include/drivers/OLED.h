#pragma once

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "config/PinMap.h"

//Configure OLED Display
namespace OLEDScreenConfig {
    // --- Connection Settings ---
    static constexpr uint8_t DEFAULT_ADDRESS = 0x3C;
    static constexpr uint32_t BUS_SPEED      = 100000; // 400kHz standard, need same speed on same wire

    // --- Opperating Parameter Constraints ---
    static constexpr uint8_t SCREEN_WIDTH = 128;
    static constexpr uint8_t SCREEN_HEIGHT = 32;
    static constexpr uint8_t OLED_RESET = -1;
}

class OledManager {
public:
    OledManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address, uint8_t width, uint8_t height);
    
    void begin(TwoWire* i2cBus = &Wire);
    void showBootScreen(const char* version);
    void updateTelemetry(float voltage, float current, float temp);
    void showError(const char* msg);

    // Expose underlying display for higher-level UI controllers. Callers
    // must check for nullptr before use.
    Adafruit_SSD1306* getDisplay() const { return _display; }

private:
    uint8_t _address;
    uint8_t _width;
    uint8_t _height;
    TwoWire* _bus;
    Adafruit_SSD1306* _display; 
};

// Extern declaration so 'display' is available globally
extern OledManager oled;
