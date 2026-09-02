#include "drivers/NeoPixel.h"
#include <Arduino.h>

NeoPixelManager::NeoPixelManager(uint8_t pin, uint16_t numpixels, neoPixelType type)
    : _strip(numpixels, pin, type), _currentState(NeoPixelState::OFF),
      _previousMillis(0), _ledOn(false), _blinkColor(COLOR_ERROR),
      _patternCount(0), _activeCount(0),
      _currentErrorIndex(0), _currentPatternIndex(0), _blinkStartMillis(0) {
          for(int i=0; i<MAX_ACTIVE; i++) _activeCodes[i] = 0;
      }

void NeoPixelManager::begin() {
    _strip.begin();
    _strip.setBrightness(50);
    _strip.show(); 
}

void NeoPixelManager::setState(NeoPixelState state) {
    if (_currentState == state) return;

    _currentState = state;
    _ledOn = false; 
    _currentErrorIndex = 0;
    _currentPatternIndex = 0;
    _blinkStartMillis = millis();
    _blinkColor = COLOR_ERROR; 

    switch (_currentState) {
        case NeoPixelState::OFF:   _setPixelColor(COLOR_OFF);  break;
        case NeoPixelState::INIT:  _setPixelColor(COLOR_INIT); break;
        case NeoPixelState::RUN:   _setPixelColor(COLOR_RUN);  break;
        case NeoPixelState::ERROR: _setPixelColor(COLOR_OFF);  break;
        case NeoPixelState::WARNING: _setPixelColor(COLOR_WARNING); break;
    }
}

void NeoPixelManager::setBlinkColor(uint32_t color) {
    _blinkColor = color;
}

void NeoPixelManager::update() {
    unsigned long currentMillis = millis();

    // Only update if in ERROR state OR in INIT with active error patterns
    if (_currentState == NeoPixelState::ERROR || (_currentState == NeoPixelState::INIT && _activeCount > 0)) {
        if (_activeCount == 0) {
            // Generic blink for generic ERROR state
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

void NeoPixelManager::_setPixelColor(uint32_t color) {
    for (int i = 0; i < _strip.numPixels(); i++) {
        _strip.setPixelColor(i, color);
    }
    _strip.show();
}

void NeoPixelManager::_displayNextErrorBit() {
    unsigned long currentMillis = millis();
    if (_activeCount == 0) return;
    
    uint8_t currentErrorCode = _activeCodes[_currentErrorIndex];
    const char* pattern = _getPattern(currentErrorCode);

    if (!pattern) {
        _currentErrorIndex = (_currentErrorIndex + 1) % _activeCount;
        _currentPatternIndex = 0;
        return;
    }

    if (_ledOn) {
        char bit = pattern[_currentPatternIndex];
        unsigned long duration = (bit == '1') ? BLINK_LONG_PULSE : BLINK_SHORT_PULSE;
        if (currentMillis - _blinkStartMillis >= duration) {
            _setPixelColor(COLOR_OFF);
            _ledOn = false;
            _blinkStartMillis = currentMillis;
            _currentPatternIndex++;
        }
    } else {
        if (pattern[_currentPatternIndex] == '\0') {
            if (currentMillis - _blinkStartMillis >= BLINK_INTER_CODE_PAUSE) {
                _currentErrorIndex = (_currentErrorIndex + 1) % _activeCount;
                _currentPatternIndex = 0;
                _blinkStartMillis = currentMillis;
            }
        } else {
            if (currentMillis - _blinkStartMillis >= BLINK_INTER_BIT_PAUSE) {
                _setPixelColor(_blinkColor);
                _ledOn = true;
                _blinkStartMillis = currentMillis;
            }
        }
    }
}

const char* NeoPixelManager::_getPattern(uint8_t code) {
    for (uint8_t i = 0; i < _patternCount; i++) {
        if (_patterns[i].code == code) return _patterns[i].pattern;
    }
    return nullptr;
}

void NeoPixelManager::registerErrorCode(uint8_t code, const char* pattern) {
    if (_patternCount < MAX_PATTERNS) {
        _patterns[_patternCount++] = {code, pattern};
    }
}

void NeoPixelManager::activateErrorCode(uint8_t code) {
    if (code == 0) return;
    for (uint8_t i = 0; i < _activeCount; i++) {
        if (_activeCodes[i] == code) return;
    }
    if (_activeCount < MAX_ACTIVE) {
        _activeCodes[_activeCount++] = code;
    }
}

void NeoPixelManager::deactivateErrorCode(uint8_t code) {
    for (uint8_t i = 0; i < _activeCount; i++) {
        if (_activeCodes[i] == code) {
            for (uint8_t j = i; j < _activeCount - 1; j++) {
                _activeCodes[j] = _activeCodes[j + 1];
            }
            _activeCount--;
            break;
        }
    }
    if (_activeCount == 0 && _currentState == NeoPixelState::ERROR) {
        setState(NeoPixelState::RUN);
    }
}

void NeoPixelManager::clearAllErrorCodes() {
    _activeCount = 0;
    _currentErrorIndex = 0;
    _currentPatternIndex = 0;
    _ledOn = false;
    setState(NeoPixelState::RUN);
}
