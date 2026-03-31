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
      _state{}, _lastUpdateMs(0), _ledIntegral(0.0f), _waterIntegral(0.0f)
{}

void CoolingService::begin() {
    analogWriteResolution(8);
    _lastUpdateMs = millis();
    _ledIntegral = 0.0f;
    _waterIntegral = 0.0f;
}

void CoolingService::update(unsigned long now) {
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

    // 3) PI Controller for LED Cooling
    float ledError = ledTempC - FanCurveConfig::TARGET_TEMP_LED;
    
    if (ledError > 0 || _ledIntegral > 0) {
        _ledIntegral += ledError * dt;
    }
    
    if (_ledIntegral > FanCurveConfig::LED_INTEGRAL_MAX) _ledIntegral = FanCurveConfig::LED_INTEGRAL_MAX;
    if (_ledIntegral < 0.0f) _ledIntegral = 0.0f;

    float ledPI_Output = (FanCurveConfig::LED_KP * ledError) + (FanCurveConfig::LED_KI * _ledIntegral);
    
    float mainFanFloat = 0;
    if (ledError > 0 || _ledIntegral > 0) {
        mainFanFloat = FanCurveConfig::MAIN_DUTY_MIN + ledPI_Output;
    }
    if (mainFanFloat > FanCurveConfig::MAIN_DUTY_MAX) mainFanFloat = FanCurveConfig::MAIN_DUTY_MAX;
    if (mainFanFloat < 0) mainFanFloat = 0;
    
    float auxFanFloat = 0;
    if (ledError > 0 || _ledIntegral > 0) {
        auxFanFloat = FanCurveConfig::AUX_DUTY_MIN + ledPI_Output;
    }
    if (auxFanFloat > FanCurveConfig::AUX_DUTY_MAX) auxFanFloat = FanCurveConfig::AUX_DUTY_MAX;
    if (auxFanFloat < 0) auxFanFloat = 0;

    uint8_t mainFanDuty = static_cast<uint8_t>(mainFanFloat);
    uint8_t auxDuty = static_cast<uint8_t>(auxFanFloat);
    
    // Safety check: only run fans if reading is valid (>0 and <100)
    if (ledTempC > 1.0f && ledTempC < 100.0f) {
        _mainFan.setDuty(mainFanDuty);
        _psuFan.setDuty(mainFanDuty);
        _auxFan.setDuty(auxDuty);
    } else {
        _mainFan.setDuty(0);
        _psuFan.setDuty(0);
        _auxFan.setDuty(0);
    }

    // 4) PI Controller for Water Cooling (Pump)
    float waterError = waterTempC - FanCurveConfig::TARGET_TEMP_WATER;

    if (waterError > 0 || _waterIntegral > 0) {
        _waterIntegral += waterError * dt;
    }

    if (_waterIntegral > FanCurveConfig::WATER_INTEGRAL_MAX) _waterIntegral = FanCurveConfig::WATER_INTEGRAL_MAX;
    if (_waterIntegral < 0.0f) _waterIntegral = 0.0f;

    float waterPI_Output = (FanCurveConfig::WATER_KP * waterError) + (FanCurveConfig::WATER_KI * _waterIntegral);
    
    float pumpFloat = 0;
    if (waterError > 0 || _waterIntegral > 0) {
        pumpFloat = FanCurveConfig::PUMP_DUTY_MIN + waterPI_Output;
    }
    if (pumpFloat > FanCurveConfig::PUMP_DUTY_MAX) pumpFloat = FanCurveConfig::PUMP_DUTY_MAX;
    if (pumpFloat < 0) pumpFloat = 0;

    uint8_t pumpDuty = static_cast<uint8_t>(pumpFloat);
    
    if (waterTempC > 1.0f && waterTempC < 100.0f) {
        _pump.setDuty(pumpDuty);
    } else {
        _pump.setDuty(0);
    }

    // 5) Read RPM values
    _state.mainFanRPM = _mainFan.getRPM();
    _state.auxFanRPM  = _auxFan.getRPM();
    _state.psuFanRPM  = _psuFan.getRPM();
    _state.pumpRPM    = _pump.getRPM();
}
