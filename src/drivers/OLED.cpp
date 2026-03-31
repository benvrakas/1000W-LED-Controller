#include "drivers/OLED.h"
#include <Arduino.h>

OledManager::OledManager(uint8_t sdaPin, uint8_t sclPin, uint8_t address, uint8_t width, uint8_t height)
    : _address(address), _width(width), _height(height), _bus(nullptr), _display(nullptr), _initialized(false) 
{}

void OledManager::begin(TwoWire* i2cBus) {
    _bus = i2cBus;
    
    if (!_display) {
        // SH1107 constructor: (height, width, wire, rst)
        // The 128x64 FeatherWing uses 64x128 in the constructor
        _display = new Adafruit_SH1107(_height, _width, _bus, -1);
    }
    
    Serial.println(F("OLED: Initializing SH1107 at 0x3C..."));
    delay(100);

    if (_display->begin(_address, true)) { // address, reset
        _display->display(); // Show Adafruit splash briefly
        delay(500);
        _display->clearDisplay();
        _display->setRotation(1); // Landscape mode for FeatherWing
        _display->setTextColor(SH110X_WHITE);
        _display->setTextSize(1);
        _display->setCursor(0, 0);
        _display->println(F("SH1107 OK!"));
        _display->display();
        
        _initialized = true;
        Serial.println(F("OLED: SH1107 Found and Initialized"));
    } else {
        _initialized = false;
        Serial.println(F("OLED: SH1107 NOT FOUND"));
    }
}

void OledManager::showBootScreen(const char* version) {
    if (!_initialized || !_display) return;
    _display->clearDisplay();
    _display->setTextColor(SH110X_WHITE);
    _display->setTextSize(2);
    _display->setCursor(10, 10);
    _display->print(F("PROJECTOR"));
    _display->setTextSize(1);
    _display->setCursor(10, 35);
    _display->print(F("v"));
    _display->print(version);
    _display->display();
}

void OledManager::updateTelemetry(float voltage, float current, float temp) {
    if (!_initialized || !_display) return;
    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setTextColor(SH110X_WHITE);
    _display->setCursor(0, 0);
    _display->print(F("V: ")); _display->print(voltage, 1);
    _display->print(F("  A: ")); _display->println(current, 1);
    _display->print(F("T: ")); _display->print(temp, 1); _display->println(F(" C"));
    _display->display();
}

void OledManager::showStatus(const char* title, const char* msg) {
    if (!_initialized || !_display) return;
    _display->clearDisplay();
    _display->setTextSize(2);
    _display->setTextColor(SH110X_WHITE);
    _display->setCursor(0, 5);
    _display->println(title);
    _display->setTextSize(1);
    _display->setCursor(0, 30);
    _display->println(msg);
    _display->display();
}

void OledManager::showError(const char* msg) {
    if (!_initialized || !_display) return;
    _display->clearDisplay();
    _display->setTextSize(2);
    _display->setTextColor(SH110X_WHITE);
    _display->setCursor(0, 5);
    _display->println(F("ERROR!"));
    _display->setTextSize(1);
    _display->setCursor(0, 30);
    _display->println(msg);
    _display->display();
}
