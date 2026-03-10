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
    // Ensure PWM writes use 8-bit resolution for all fan/pump channels.
    analogWriteResolution(8);
    
    _lastUpdateMs = millis();
    _ledIntegral = 0.0f;
    _waterIntegral = 0.0f;
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

    // Time delta for integral calculation
    float dt = (now - _lastUpdateMs) / 1000.0f;
    if (dt <= 0.0f || dt > 1.0f) dt = 0.1f; // Prevent huge leaps on delays or first cycle
    _lastUpdateMs = now;

    // 3) PI Controller for LED Cooling (Radiator Fans, PSU Fan, Aux Fan)
    float ledError = ledTempC - FanCurveConfig::TARGET_TEMP_LED;
    
    // Only accumulate integral when error is positive (we only cool, we don't heat)
    // Or allow negative error to decrease integral back to 0
    if (ledError > 0 || _ledIntegral > 0) {
        _ledIntegral += ledError * dt;
    }
    
    // Clamp integral to prevent windup
    if (_ledIntegral > FanCurveConfig::LED_INTEGRAL_MAX) _ledIntegral = FanCurveConfig::LED_INTEGRAL_MAX;
    if (_ledIntegral < 0.0f) _ledIntegral = 0.0f;

    float ledPI_Output = (FanCurveConfig::LED_KP * ledError) + (FanCurveConfig::LED_KI * _ledIntegral);
    
    // Calculate final duties based on minimums + PI output
    float mainFanFloat = FanCurveConfig::MAIN_DUTY_MIN + ledPI_Output;
    if (mainFanFloat > FanCurveConfig::MAIN_DUTY_MAX) mainFanFloat = FanCurveConfig::MAIN_DUTY_MAX;
    if (mainFanFloat < FanCurveConfig::MAIN_DUTY_MIN) mainFanFloat = FanCurveConfig::MAIN_DUTY_MIN;
    
    float auxFanFloat = FanCurveConfig::AUX_DUTY_MIN + ledPI_Output;
    if (auxFanFloat > FanCurveConfig::AUX_DUTY_MAX) auxFanFloat = FanCurveConfig::AUX_DUTY_MAX;
    if (auxFanFloat < FanCurveConfig::AUX_DUTY_MIN) auxFanFloat = FanCurveConfig::AUX_DUTY_MIN;

    uint8_t mainFanDuty = static_cast<uint8_t>(mainFanFloat);
    uint8_t auxDuty = static_cast<uint8_t>(auxFanFloat);
    
    _mainFan.setDuty(mainFanDuty);
    _psuFan.setDuty(mainFanDuty); // PSU fan follows main fan
    _auxFan.setDuty(auxDuty);


    // 4) PI Controller for Water Cooling (Pump)
    float waterError = waterTempC - FanCurveConfig::TARGET_TEMP_WATER;

    if (waterError > 0 || _waterIntegral > 0) {
        _waterIntegral += waterError * dt;
    }

    if (_waterIntegral > FanCurveConfig::WATER_INTEGRAL_MAX) _waterIntegral = FanCurveConfig::WATER_INTEGRAL_MAX;
    if (_waterIntegral < 0.0f) _waterIntegral = 0.0f;

    float waterPI_Output = (FanCurveConfig::WATER_KP * waterError) + (FanCurveConfig::WATER_KI * _waterIntegral);
    
    float pumpFloat = FanCurveConfig::PUMP_DUTY_MIN + waterPI_Output;
    if (pumpFloat > FanCurveConfig::PUMP_DUTY_MAX) pumpFloat = FanCurveConfig::PUMP_DUTY_MAX;
    if (pumpFloat < FanCurveConfig::PUMP_DUTY_MIN) pumpFloat = FanCurveConfig::PUMP_DUTY_MIN;

    uint8_t pumpDuty = static_cast<uint8_t>(pumpFloat);
    _pump.setDuty(pumpDuty);

    // 5) Read RPM values from tachometer managers
    _state.mainFanRPM = _mainFan.getRPM();
    _state.auxFanRPM  = _auxFan.getRPM();
    _state.psuFanRPM  = _psuFan.getRPM();
    _state.pumpRPM    = _pump.getRPM();
}
