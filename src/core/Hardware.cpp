#include "core/Hardware.h"
#include "config/PinMap.h"
#include "config/PowerConfig.h"
#include "config/ThermalConfig.h"
#include "logging/FaultManager.h"

PowerButtonManager powerButton(PinMap::PIN_SW_BTN, PinMap::PIN_SW_LED);

TachometerManager pump(PinMap::PIN_PUMP_PWM, PinMap::PIN_PUMP_TACH,
    TachometerConfig::PUMP_DEADSTART_DUTY, TachometerConfig::MAX_PUMP_RPM,
    TachometerConfig::PUMP_STALL_RPM);

TachometerManager mainFan(PinMap::PIN_RAD_FANS_PWM, PinMap::PIN_RAD_FAN_TACH,
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM,
    TachometerConfig::MAIN_PSU_STALL_RPM);

TachometerManager psuFan(PinMap::PIN_PSU_FAN_PWM, PinMap::PIN_PSU_FAN_TACH,
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM,
    TachometerConfig::MAIN_PSU_STALL_RPM);

// NMB 12038VA-24R datasheet specs PWM Frequency f = 25kHz and documents
// "Vst = Open -> Full Speed" as its fallback when it can't decode the
// signal - the core's default PWM frequency (~1.8kHz) isn't close enough,
// which is why this fan pins at full speed while other models on the same
// port work fine.
TachometerManager auxFan(PinMap::PIN_AUX_FAN_PWM, PinMap::PIN_AUX_FAN_TACH,
    TachometerConfig::AUX_DEADSTART_DUTY, TachometerConfig::MAX_AUX_RPM,
    TachometerConfig::AUX_STALL_RPM, 25000);

ThermistorManager ledThermistor(PinMap::PIN_THERM_LED, ThermistorConfig::BETA_VALUE_LED, 
    ThermistorConfig::SERIES_RESISTOR_LED, ThermistorConfig::NOMINAL_RESISTANCE_LED, 
    ThermistorConfig::IS_HIGH_SIDE_LED);

ThermistorManager pumpThermistor(PinMap::PIN_THERM_WATER, ThermistorConfig::BETA_VALUE_PUMP, 
    ThermistorConfig::SERIES_RESISTOR_PUMP, ThermistorConfig::NOMINAL_RESISTANCE_PUMP,
    ThermistorConfig::IS_HIGH_SIDE_PUMP);

CanBusManager psu;
NativeCanBackend canBackend;
AnalogPsuBackend psuAnalog(PinMap::PIN_PSU_PC_PWM);

OledManager oled(PinMap::PIN_OLED_SDA, PinMap::PIN_OLED_SCL, OLEDScreenConfig::DEFAULT_ADDRESS,
    OLEDScreenConfig::SCREEN_WIDTH, OLEDScreenConfig::SCREEN_HEIGHT);

NeoPixelManager neoPixel(PinMap::PIN_STATUS_LED, 1, NEO_GRB + NEO_KHZ800);

void initHardware() {
    Serial.println(F("HW: OLED init..."));
    Wire.begin();
    Wire.setClock(400000); // 400kHz I2C
    Wire.setTimeout(100);  // 100ms timeout
    oled.begin(&Wire);

    if (PsuControlConfig::PSU_CONTROL_VIA_CAN) {
        Serial.println(F("HW: CAN init..."));
        psu.begin(&canBackend);
    } else {
        Serial.println(F("HW: Analog PSU mode (CAN driver idle)"));
        psuAnalog.begin();
    }

    Serial.println(F("HW: NeoPixel init..."));
    neoPixel.begin();
    
    neoPixel.registerErrorCode((uint8_t)FaultCode::CAN_TIMEOUT,     "10");
    neoPixel.registerErrorCode((uint8_t)FaultCode::PSU_FAULT,       "11");
    neoPixel.registerErrorCode((uint8_t)FaultCode::OVER_TEMP_LED,   "010");
    neoPixel.registerErrorCode((uint8_t)FaultCode::OVER_TEMP_WATER, "011");
    neoPixel.registerErrorCode((uint8_t)FaultCode::COOLING_FAILURE, "00");
    neoPixel.registerErrorCode((uint8_t)FaultCode::ENCODER_FAULT,   "001");
    neoPixel.registerErrorCode((uint8_t)FaultCode::INIT_FAILED,     "1");

    neoPixel.registerErrorCode(101, "0");
    neoPixel.registerErrorCode(102, "00");
    neoPixel.registerErrorCode(103, "000");
    neoPixel.registerErrorCode(104, "1");
    neoPixel.registerErrorCode(105, "11");

    neoPixel.setState(NeoPixelState::INIT);
}
