#include <Adafruit_SleepyDog.h>
#include "state/SystemController.h"
#include "core/Hardware.h"

// Construct AppContext with references to global hardware instances
// Order must match AppContext struct definition in include/core/AppContext.h
AppContext context = {
    pump,           // TachometerManager& pump
    mainFan,        // TachometerManager& mainFan
    psuFan,         // TachometerManager& psuFan
    auxFan,         // TachometerManager& auxFan
    ledThermistor,  // ThermistorManager& ledThermistor
    pumpThermistor, // ThermistorManager& pumpThermistor
    encoder,        // EncoderManager& encoder
    powerButton,    // PowerButtonManager& powerButton
    psu,            // CanBusManager& psu
    psuAnalog,      // AnalogPsuBackend& psuAnalog
    oled,           // OledManager& oled
    neoPixel        // NeoPixelManager& neoPixel
};

// Initialize System Controller with dependency injection
SystemController sys(context);

void setup() {
    Watchdog.enable(4000); // Increased to 4s to allow for Flash/OLED operations
    Serial.begin(115200);
    
    // SAMD51 specific: set ADC to 12-bit precision globally
    analogReadResolution(12);

    // Wait for USB Serial to connect, but don't hang forever
    while (!Serial && millis() < 2000) {
        delay(10);
    }
    Serial.println("System Initializing...");

    // Initialize bus-level hardware (I2C, SPI, CAN backend)
    initHardware();

    // Begin system state machine
    sys.begin();
}

void loop() {
    sys.update();
    Watchdog.reset();
}
