#pragma once

#include <Adafruit_NeoPixel.h>

// Enum for system states to control NeoPixel color
enum class NeoPixelState {
    OFF,
    INIT,
    RUN,
    ERROR
};

// NeoPixelManager class to handle the onboard NeoPixel LED
class NeoPixelManager {
public:
    // Constructor
    NeoPixelManager(uint8_t pin, uint16_t numpixels, neoPixelType type);

    // Initialization
    void begin();

    // Set the NeoPixel to a specific state (color)
    void setState(NeoPixelState state);

    // Set custom color for blinking (e.g., BLUE for init failure)
    void setBlinkColor(uint32_t color);

    // Update method to be called in the main loop for blinking animations
    void update();

    // Register an error code with its binary pattern (Short/Long pulses)
    void registerErrorCode(uint8_t code, const char* pattern);

    // Activate an error code for display
    void activateErrorCode(uint8_t code);

    // Deactivate an error code
    void deactivateErrorCode(uint8_t code);

    // Clear all active error codes
    void clearAllErrorCodes();

private:
    Adafruit_NeoPixel _strip;
    NeoPixelState _currentState;

    // State colors
    static const uint32_t COLOR_OFF = 0x000000;  // Black
    static const uint32_t COLOR_INIT = 0x0000FF; // Blue
    static const uint32_t COLOR_RUN = 0x00FF00;  // Green
    static const uint32_t COLOR_ERROR = 0xFF0000; // Red

    // Blinking parameters
    unsigned long _previousMillis;
    bool _ledOn;
    uint32_t _blinkColor;

    // Fixed storage to avoid heap fragmentation from map/vector in tight loops
    static constexpr uint8_t MAX_PATTERNS = 20;
    struct ErrorPattern {
        uint8_t code;
        const char* pattern;
    } _patterns[MAX_PATTERNS];
    uint8_t _patternCount;

    static constexpr uint8_t MAX_ACTIVE = 4;
    uint8_t _activeCodes[MAX_ACTIVE];
    uint8_t _activeCount;

    size_t _currentErrorIndex; 
    int _currentPatternIndex;  
    unsigned long _blinkStartMillis; 

    static const unsigned long BLINK_SHORT_PULSE = 300;  
    static const unsigned long BLINK_LONG_PULSE = 600;   
    static const unsigned long BLINK_INTER_BIT_PAUSE = 150; 
    static const unsigned long BLINK_INTER_CODE_PAUSE = 1000; 

    // Helpers
    void _setPixelColor(uint32_t color);
    void _displayNextErrorBit();
    const char* _getPattern(uint8_t code);
};
