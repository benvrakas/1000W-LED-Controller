#include "services/CoolingService.h"

#include <Arduino.h>

#include "util/BoardPins.h"
#include "drivers/Thermistors.h"
#include "drivers/Tachometers.h"

// Global thermistor instances from main.cpp
extern ThermistorManager ledThermistor;
extern ThermistorManager pumpThermistor;

// Global tachometer instances from main.cpp
extern TachometerManager mainFan;
extern TachometerManager auxFan;
extern TachometerManager psuFan;
extern TachometerManager pump;

// Helper: map a temperature into an 8-bit duty cycle with clamping.
static uint8_t mapTempToDuty(float tempC,
                             float tLow, float tHigh,
                             uint8_t dutyMin, uint8_t dutyMax) {
    if (tempC <= tLow) {
        return dutyMin;
    }
    if (tempC >= tHigh) {
        return dutyMax;
    }

    float frac = (tempC - tLow) / (tHigh - tLow);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    float duty = dutyMin + frac * static_cast<float>(dutyMax - dutyMin);
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 255.0f) duty = 255.0f;
    return static_cast<uint8_t>(duty + 0.5f);
}

CoolingService::CoolingService()
    : _state{} {}

void CoolingService::begin() {
    // Ensure PWM writes use 8-bit resolution for all fan/pump channels.
    analogWriteResolution(8);
}

void CoolingService::update(unsigned long now) {
    // 1) Update tachometer RPM calculations
    mainFan.update(now);
    auxFan.update(now);
    psuFan.update(now);
    pump.update(now);

    // 2) Read current temperatures from thermistor managers
    float ledTempC   = ledThermistor.getCelsius();
    float waterTempC = pumpThermistor.getCelsius();

    // Store in internal state for external access via getState()
    _state.ledTempC   = ledTempC;
    _state.waterTempC = waterTempC;

    // 3) Compute PWM duties using fan curves (temperature-based)

    // Radiator fans + PSU fan driven primarily by LED temperature.
    // Quiet (~25%) below 35C, ramping to full by 75C.
    uint8_t mainFanDuty = mapTempToDuty(
        ledTempC,
        35.0f, 75.0f,   // temperature window
        64, 255         // duty window (~25% to 100%)
    );
    mainFan.setDuty(mainFanDuty);

    // PSU fan tied to same curve as radiator fans
    uint8_t psuFanDuty = mainFanDuty;
    psuFan.setDuty(psuFanDuty);

    // Pump driven by water temperature.
    // Baseline ~30% at 30C, ramp to full by 50C.
    uint8_t pumpDuty = mapTempToDuty(
        waterTempC,
        30.0f, 50.0f,
        77, 255  // ~30% to 100%
    );
    pump.setDuty(pumpDuty);

    // Aux (lens) fan based on LED temperature for now.
    // Baseline ~20% at 30C, ramp to full by 75C.
    uint8_t auxDuty = mapTempToDuty(
        ledTempC,
        30.0f, 75.0f,
        51, 255  // ~20% to 100%
    );
    auxFan.setDuty(auxDuty);

    // 4) Read RPM values from tachometer managers
    _state.mainFanRPM = mainFan.getRPM();
    _state.auxFanRPM  = auxFan.getRPM();
    _state.psuFanRPM  = psuFan.getRPM();
    _state.pumpRPM    = pump.getRPM();
}
