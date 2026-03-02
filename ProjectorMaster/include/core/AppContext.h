#pragma once

#include "drivers/Tachometers.h"
#include "drivers/Thermistors.h"
#include "drivers/CanBus.h"
#include "drivers/OLED.h"
#include "drivers/PowerButton.h"
#include "drivers/Encoder.h"

// AppContext
// ----------
// Dependency Injection container holding references to all hardware drivers.
// Passed to Services and Controllers to avoid usage of global variables.

struct AppContext {
    // Tachometers
    TachometerManager& pump;
    TachometerManager& mainFan;
    TachometerManager& psuFan;
    TachometerManager& auxFan;

    // Sensors
    ThermistorManager& ledThermistor;
    ThermistorManager& pumpThermistor;
    EncoderManager&    encoder;
    PowerButtonManager& powerButton;

    // Actuators / Comms
    CanBusManager&     psu;
    OledManager&       oled;
};
