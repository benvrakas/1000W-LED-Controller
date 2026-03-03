#include "services/CoolingService.h"

#include <Arduino.h>

#include "config/PinMap.h"
#include "drivers/Thermistors.h"
#include "drivers/Tachometers.h"
#include "config/ThermalConfig.h"

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

CoolingService::CoolingService(TachometerManager& mainFan, TachometerManager& psuFan,
                               TachometerManager& pump, TachometerManager& auxFan,
                               ThermistorManager& ledThermistor, ThermistorManager& pumpThermistor)
    : _mainFan(mainFan), _psuFan(psuFan), _pump(pump), _auxFan(auxFan),
      _ledThermistor(ledThermistor), _pumpThermistor(pumpThermistor),
      _state{}
{}

void CoolingService::begin() {
    // Ensure PWM writes use 8-bit resolution for all fan/pump channels.
    analogWriteResolution(8);
}

void CoolingService::update(unsigned long now) {
    // 1) Update tachometer RPM calculations
    _mainFan.update(now);
    _auxFan.update(now);
    _psuFan.update(now);
    _pump.update(now);

    // 2) Read current temperatures from thermistor managers
    float ledTempC   = _ledThermistor.getCelsius();
    float waterTempC = _pumpThermistor.getCelsius();

    // Store in internal state for external access via getState()
    _state.ledTempC   = ledTempC;
    _state.waterTempC = waterTempC;

    // 3) Compute PWM duties using fan curves (temperature-based)

    // Radiator fans + PSU fan driven primarily by LED temperature.
    uint8_t mainFanDuty = mapTempToDuty(
        ledTempC,
        FanCurveConfig::MAIN_TEMP_MIN, FanCurveConfig::MAIN_TEMP_MAX,
        FanCurveConfig::MAIN_DUTY_MIN, FanCurveConfig::MAIN_DUTY_MAX
    );
    _mainFan.setDuty(mainFanDuty);

    // PSU fan tied to same curve as radiator fans
    uint8_t psuFanDuty = mainFanDuty;
    _psuFan.setDuty(psuFanDuty);

    // Pump driven by water temperature.
    uint8_t pumpDuty = mapTempToDuty(
        waterTempC,
        FanCurveConfig::PUMP_TEMP_MIN, FanCurveConfig::PUMP_TEMP_MAX,
        FanCurveConfig::PUMP_DUTY_MIN, FanCurveConfig::PUMP_DUTY_MAX
    );
    _pump.setDuty(pumpDuty);

    // Aux (lens) fan based on LED temperature for now.
    uint8_t auxDuty = mapTempToDuty(
        ledTempC,
        FanCurveConfig::AUX_TEMP_MIN, FanCurveConfig::AUX_TEMP_MAX,
        FanCurveConfig::AUX_DUTY_MIN, FanCurveConfig::AUX_DUTY_MAX
    );
    _auxFan.setDuty(auxDuty);

    // 4) Read RPM values from tachometer managers
    _state.mainFanRPM = _mainFan.getRPM();
    _state.auxFanRPM  = _auxFan.getRPM();
    _state.psuFanRPM  = _psuFan.getRPM();
    _state.pumpRPM    = _pump.getRPM();
}
