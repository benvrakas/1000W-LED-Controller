#include "drivers/NeoPixel.h"
#include <Arduino.h>
#include <algorithm>

// Constructor
NeoPixelManager::NeoPixelManager(uint8_t pin, uint16_t numpixels, neoPixelType type)
    : _strip(numpixels, pin, type), _currentState(NeoPixelState::OFF),
      _previousMillis(0), _ledOn(false), _currentErrorIndex(0), _currentPatternIndex(0),
      _blinkStartMillis(0), _blinkColor(COLOR_ERROR) {}

// Initialization
void NeoPixelManager::begin() {
    _strip.begin();
    _strip.setBrightness(50); // Moderate brightness
    _strip.show(); // Initialize all pixels to 'off'
}

// Set the NeoPixel to a specific state (color)
void NeoPixelManager::setState(NeoPixelState state) {
    if (_currentState == state) return;

    _currentState = state;
    _ledOn = false; 
    _currentErrorIndex = 0;
    _currentPatternIndex = 0;
    _blinkStartMillis = millis();
    _blinkColor = COLOR_ERROR; // Reset to red error by default

    switch (_currentState) {
        case NeoPixelState::OFF:
            _setPixelColor(COLOR_OFF);
            break;
        case NeoPixelState::INIT:
            _setPixelColor(COLOR_INIT);
            break;
        case NeoPixelState::RUN:
            _setPixelColor(COLOR_RUN);
            break;
        case NeoPixelState::ERROR:
            _setPixelColor(COLOR_OFF);
            break;
    }
}

void NeoPixelManager::setBlinkColor(uint32_t color) {
    _blinkColor = color;
}

// Update method to be called in the main loop for blinking animations
void NeoPixelManager::update() {
    unsigned long currentMillis = millis();

    if (_currentState == NeoPixelState::ERROR || (_currentState == NeoPixelState::INIT && !_activeErrorCodes.empty())) {
        if (_activeErrorCodes.empty()) {
            // Generic blink if no specific codes active (Error state only)
            if (_currentState == NeoPixelState::ERROR) {
                if (currentMillis - _previousMillis >= BLINK_SHORT_PULSE) {
                    _previousMillis = currentMillis;
                    _ledOn = !_ledOn;
                    _setPixelColor(_ledOn ? _blinkColor : COLOR_OFF);
                }
            }
        } else {
            _displayNextErrorBit();
        }
    }
}

// Helper to set pixel color and show
void NeoPixelManager::_setPixelColor(uint32_t color) {
    for (int i = 0; i < _strip.numPixels(); i++) {
        _strip.setPixelColor(i, color);
    }
    _strip.show();
}

void NeoPixelManager::_displayNextErrorBit() {
    unsigned long currentMillis = millis();
    
    if (_activeErrorCodes.empty()) return;
    
    uint8_t currentErrorCode = _activeErrorCodes[_currentErrorIndex];
    
    if (_errorPatterns.find(currentErrorCode) == _errorPatterns.end()) {
        _currentErrorIndex = (_currentErrorIndex + 1) % _activeErrorCodes.size();
        return;
    }
    
    std::string pattern = _errorPatterns[currentErrorCode];

    if (_ledOn) {
        // LED is currently showing a bit
        char bit = pattern[_currentPatternIndex];
        unsigned long duration = (bit == '1') ? BLINK_LONG_PULSE : BLINK_SHORT_PULSE;
        
        if (currentMillis - _blinkStartMillis >= duration) {
            _setPixelColor(COLOR_OFF);
            _ledOn = false;
            _blinkStartMillis = currentMillis;
            _currentPatternIndex++;
        }
    } else {
        // LED is OFF (inter-bit or inter-code pause)
        if (_currentPatternIndex >= (int)pattern.length()) {
            // Inter-code pause
            if (currentMillis - _blinkStartMillis >= BLINK_INTER_CODE_PAUSE) {
                _currentErrorIndex = (_currentErrorIndex + 1) % _activeErrorCodes.size();
                _currentPatternIndex = 0;
                _blinkStartMillis = currentMillis;
            }
        } else {
            // Inter-bit pause
            if (currentMillis - _blinkStartMillis >= BLINK_INTER_BIT_PAUSE) {
                _setPixelColor(_blinkColor);
                _ledOn = true;
                _blinkStartMillis = currentMillis;
            }
        }
    }
}

// Register an error code with its binary pattern
void NeoPixelManager::registerErrorCode(uint8_t code, const std::string& pattern) {
    _errorPatterns[code] = pattern;
}

// Activate an error code for display
void NeoPixelManager::activateErrorCode(uint8_t code) {
    bool found = false;
    for(uint8_t active : _activeErrorCodes) {
        if(active == code) {
            found = true;
            break;
        }
    }
    if (!found) {
        _activeErrorCodes.push_back(code);
    }
}

// Deactivate an error code
void NeoPixelManager::deactivateErrorCode(uint8_t code) {
    _activeErrorCodes.erase(std::remove(_activeErrorCodes.begin(), _activeErrorCodes.end(), code), _activeErrorCodes.end());
    if (_activeErrorCodes.empty() && _currentState == NeoPixelState::ERROR) {
        setState(NeoPixelState::RUN);
    }
}

// Clear all active error codes
void NeoPixelManager::clearAllErrorCodes() {
    _activeErrorCodes.clear();
    _currentErrorIndex = 0;
    _currentPatternIndex = 0;
    _ledOn = false;
    setState(NeoPixelState::RUN);
}
