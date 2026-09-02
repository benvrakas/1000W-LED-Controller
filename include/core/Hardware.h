#pragma once

#include "drivers/Tachometers.h"
#include "drivers/Thermistors.h"
#include "drivers/CanBus.h"
#include "drivers/NativeCanBackend.h"
#include "drivers/AnalogPsuBackend.h"
#include "drivers/OLED.h"
#include "drivers/PowerButton.h"
#include "drivers/Encoder.h"
#include <Wire.h>
#include "drivers/NeoPixel.h"

// Global Hardware Instances
// (Extern declarations are also in their respective driver headers, 
// but collected here for central access if needed)

extern TachometerManager pump;
extern TachometerManager mainFan;
extern TachometerManager psuFan;
extern TachometerManager auxFan;
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;
extern CanBusManager psu;
extern NativeCanBackend canBackend;
extern AnalogPsuBackend psuAnalog;
extern OledManager oled;
extern NeoPixelManager neoPixel;

// Hardware Initialization
// Called from setup() to initialize buses and pins that require runtime context
void initHardware();
