#include "services/CoolingService.h"

#include <Arduino.h>

#include "config/PinMap.h"
#include "drivers/Thermistors.h"
#include "drivers/Tachometers.h"
#include "config/ThermalConfig.h"

CoolingService::CoolingService(TachometerManager& mainFan, TachometerManager& psuFan,
                               TachometerManager& pump, TachometerManager& auxFan,
                               ThermistorManager& ledThermistor, ThermistorManager& pumpThermistor)
    : _mainFan(mainFan), _psuFan(psuFan), _pump(pump), _auxFan(auxFan),
      _ledThermistor(ledThermistor), _pumpThermistor(pumpThermistor),
      _state{}, _lastUpdateMs(0), _waterIntegral(0.0f)
{}

void CoolingService::begin() {
    analogWriteResolution(8);
    _lastUpdateMs = millis();
    _waterIntegral = 0.0f;
}

void CoolingService::update(unsigned long now, float ledDutyFraction) {
    bool ledOn = ledDutyFraction > 0.0f;

    // 0) Update sensors
    _ledThermistor.updateTemp();
    _pumpThermistor.updateTemp();

    // 1) Update tachometer RPM calculations
    _mainFan.update(now);
    _auxFan.update(now);
    _psuFan.update(now);
    _pump.update(now);

    // 2) Read current temperatures
    float ledTempC   = _ledThermistor.getCelsius();
    float waterTempC = _pumpThermistor.getCelsius();

    _state.ledTempC   = ledTempC;
    _state.waterTempC = waterTempC;

    float dt = (now - _lastUpdateMs) / 1000.0f;
    if (dt <= 0.0f || dt > 1.0f) dt = 0.1f;
    _lastUpdateMs = now;

    // 3) PI Controller, targeting WATER temperature -- drives the main
    // radiator fans (and, via mainFanDuty, the aux/PSU fans' LED-off
    // fallback). Conceptually the radiator's job is dissipating heat from
    // the coolant after it's picked up heat from the LED block, so it
    // should track coolant temperature, not the LED's -- this used to
    // target TARGET_TEMP_LED instead, which was backwards. The pump has no
    // PI loop of its own anymore (see section 4 below) -- it's simply
    // maxed whenever the LED is on and idles at a fixed duty otherwise.
    float waterError = waterTempC - FanCurveConfig::TARGET_TEMP_WATER;

    if (waterError > 0 || _waterIntegral > 0) {
        _waterIntegral += waterError * dt;
    }

    if (_waterIntegral > FanCurveConfig::WATER_INTEGRAL_MAX) _waterIntegral = FanCurveConfig::WATER_INTEGRAL_MAX;
    if (_waterIntegral < 0.0f) _waterIntegral = 0.0f;

    float waterPI_Output = (FanCurveConfig::WATER_KP * waterError) + (FanCurveConfig::WATER_KI * _waterIntegral);

    float mainFanFloat = 0;
    if (waterError > 0 || _waterIntegral > 0) {
        mainFanFloat = FanCurveConfig::MAIN_DUTY_MIN + waterPI_Output;
    }
    if (mainFanFloat > FanCurveConfig::MAIN_DUTY_MAX) mainFanFloat = FanCurveConfig::MAIN_DUTY_MAX;
    if (mainFanFloat < 0) mainFanFloat = 0;

    // Lens cooling fan's LED-off fallback: unlike the radiator fan, this
    // must run continuously any time the system is armed (RUN state),
    // independent of LED temperature - it protects the lens itself, not
    // just the LED heatsink. Floor at AUX_DUTY_MIN always; boosts alongside
    // whatever's currently pushing the main fan (now the water PI output)
    // once that's over target. Only used below when the LED is off -- see
    // auxFanDuty.
    float auxFanFallbackFloat = FanCurveConfig::AUX_DUTY_MIN;
    if (waterPI_Output > 0) {
        auxFanFallbackFloat += waterPI_Output;
    }
    if (auxFanFallbackFloat > FanCurveConfig::AUX_DUTY_MAX) auxFanFallbackFloat = FanCurveConfig::AUX_DUTY_MAX;

    uint8_t mainFanDuty = static_cast<uint8_t>(mainFanFloat);

    // Safety check: only run the main fan if the WATER reading is valid
    // (>0 and <100) -- matches what it's now actually driven by.
    if (waterTempC > 1.0f && waterTempC < 100.0f) {
        _mainFan.setDuty(mainFanDuty);
    } else {
        _mainFan.setDuty(0);
    }

    // PSU fan and aux/lens fan: fixed linear relationship to LED duty while
    // the LED is on, bypassing the LED-temp PI curve (and its thermistor-
    // validity guard above) entirely -- duty is derived from the LED's
    // actual commanded current, a value we always trust, not from a
    // temperature reading that could be invalid/disconnected. Each falls
    // back to its prior behavior once the LED is off.
    uint8_t psuFanDuty;
    if (ledOn) {
        float psuFanFloat = FanCurveConfig::PSU_FAN_TO_LED_DUTY_RATIO * ledDutyFraction * 255.0f;
        if (psuFanFloat > 255.0f) psuFanFloat = 255.0f;
        if (psuFanFloat < 0.0f) psuFanFloat = 0.0f;
        psuFanDuty = static_cast<uint8_t>(psuFanFloat);
    } else if (waterTempC > 1.0f && waterTempC < 100.0f) {
        // LED off: nothing to derive the linear relationship from, fall
        // back to the same water-PI duty main fan uses (mirrors it, so
        // checks the same WATER validity main fan's own guard above does).
        psuFanDuty = mainFanDuty;
    } else {
        psuFanDuty = 0;
    }
    _psuFan.setDuty(psuFanDuty);

    uint8_t auxFanDuty;
    if (ledOn) {
        float auxFanFloat = FanCurveConfig::AUX_FAN_TO_LED_DUTY_RATIO * ledDutyFraction * 255.0f;
        if (auxFanFloat > 255.0f) auxFanFloat = 255.0f;
        if (auxFanFloat < 0.0f) auxFanFloat = 0.0f;
        auxFanDuty = static_cast<uint8_t>(auxFanFloat);
    } else {
        // LED off: fall back to the original lens-protection floor+PI
        // behavior (runs continuously whenever armed, independent of
        // thermistor validity -- it's protecting the lens, not reacting to
        // LED heatsink temperature).
        auxFanDuty = static_cast<uint8_t>(auxFanFallbackFloat);
    }
    _auxFan.setDuty(auxFanDuty);

    // 4) Pump duty. No PI loop here -- a fixed idle duty is good enough for
    // the LED-off case (the pump is quiet even at full speed, and it's
    // unconditionally maxed whenever the LED is on regardless, so a
    // temperature-reactive fallback was never doing much useful work).
    uint8_t pumpDuty;
    if (ledOn) {
        // Pump must run flat out any time the LED is actively drawing
        // power, independent of temperature.
        pumpDuty = FanCurveConfig::PUMP_DUTY_MAX;
    } else {
        pumpDuty = FanCurveConfig::PUMP_DUTY_MIN;
    }
    _pump.setDuty(pumpDuty);

    // 5) Read RPM and duty values
    _state.mainFanRPM = _mainFan.getRPM();
    _state.auxFanRPM  = _auxFan.getRPM();
    _state.psuFanRPM  = _psuFan.getRPM();
    _state.pumpRPM    = _pump.getRPM();

    _state.mainFanDuty = _mainFan.getDuty();
    _state.auxFanDuty  = _auxFan.getDuty();
    _state.psuFanDuty  = _psuFan.getDuty();
    _state.pumpDuty    = _pump.getDuty();
}
