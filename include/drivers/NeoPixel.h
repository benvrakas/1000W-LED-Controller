#pragma once

#include <Adafruit_NeoPixel.h>
#include <map>
#include <string>
#include <vector>

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

    // Register an error code with its binary pattern
    void registerErrorCode(uint8_t code, const std::string& pattern);

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

    // Error blinking specific variables
    std::map<uint8_t, std::string> _errorPatterns;
    std::vector<uint8_t> _activeErrorCodes;
    size_t _currentErrorIndex; // Index for _activeErrorCodes
    int _currentPatternIndex;  // Index for current error's pattern string
    unsigned long _blinkStartMillis; // Timestamp when current blink started

    static const unsigned long BLINK_SHORT_PULSE = 300;  // ms for '0'
    static const unsigned long BLINK_LONG_PULSE = 600;   // ms for '1'
    static const unsigned long BLINK_INTER_BIT_PAUSE = 150; // ms pause between bits
    static const unsigned long BLINK_INTER_CODE_PAUSE = 1000; // ms pause between error codes

    // Helper to set pixel color and show
    void _setPixelColor(uint32_t color);
    void _displayNextErrorBit();

    uint32_t _blinkColor;
};