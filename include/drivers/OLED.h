#pragma once

#include <Adafruit_SH110X.h>
#include <Wire.h>
#include "config/PinMap.h"

//Configure OLED Display
namespace OLEDScreenConfig {
    // --- Connection Settings ---
    static constexpr uint8_t DEFAULT_ADDRESS = 0x3C;
    static constexpr uint32_t BUS_SPEED      = 100000; 

    // --- SH1107 FeatherWing: 128x64 ---
    static constexpr uint8_t SCREEN_WIDTH = 128;
    static constexpr uint8_t SCREEN_HEIGHT = 64;
}

class OledManager {
public:
    OledManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address, uint8_t width, uint8_t height);
    
    void begin(TwoWire* i2cBus = &Wire);
    void showBootScreen(const char* version);
    void updateTelemetry(float voltage, float current, float temp);
    void showStatus(const char* title, const char* msg);
    void showError(const char* msg);

    bool isReady() const { return _initialized; }
    Adafruit_SH1107* getDisplay() const { return _display; }

private:
    uint8_t _address;
    uint8_t _width;
    uint8_t _height;
    TwoWire* _bus;
    Adafruit_SH1107* _display; 
    bool _initialized = false;
};

extern OledManager oled;
