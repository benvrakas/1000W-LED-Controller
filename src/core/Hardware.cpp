#include "core/Hardware.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
#include "logging/FaultManager.h"
#include <SPI.h>

// ---------------------------------------------------------------------------
// Global Hardware Instantiation
// ---------------------------------------------------------------------------

// Power Button
PowerButtonManager powerButton(PinMap::PIN_SW_BTN, PinMap::PIN_SW_LED);

// Tachometers (Fans & Pump)
// Note: Aux and PSU fan mapping updated to match new pinout
TachometerManager pump(PinMap::PIN_PUMP_PWM, PinMap::PIN_PUMP_TACH,
    TachometerConfig::PUMP_DEADSTART_DUTY, TachometerConfig::MAX_PUMP_RPM,
    TachometerConfig::PUMP_STALL_RPM);

TachometerManager mainFan(PinMap::PIN_RAD_FANS_PWM, PinMap::PIN_RAD_FAN_TACH,
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM,
    TachometerConfig::MAIN_PSU_STALL_RPM);

TachometerManager psuFan(PinMap::PIN_PSU_FAN_PWM, PinMap::PIN_PSU_FAN_TACH,
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM,
    TachometerConfig::MAIN_PSU_STALL_RPM);

TachometerManager auxFan(PinMap::PIN_AUX_FAN_PWM, PinMap::PIN_AUX_FAN_TACH,
    TachometerConfig::AUX_DEADSTART_DUTY, TachometerConfig::MAX_AUX_RPM,
    TachometerConfig::AUX_STALL_RPM);

// Thermistors
ThermistorManager ledThermistor(PinMap::PIN_THERM_LED, ThermistorConfig::BETA_VALUE_LED, 
    ThermistorConfig::SERIES_RESISTOR_LED);

ThermistorManager pumpThermistor(PinMap::PIN_THERM_WATER, ThermistorConfig::BETA_VALUE_PUMP, 
    ThermistorConfig::SERIES_RESISTOR_PUMP);

// CAN Bus & PSU
// Uses native CAN controller
CanBusManager psu;
NativeCanBackend canBackend;

// OLED Display
// Uses standard Wire interface (SDA/SCL)
OledManager oled(PinMap::PIN_OLED_SDA, PinMap::PIN_OLED_SCL, OLEDScreenConfig::DEFAULT_ADDRESS,
    OLEDScreenConfig::SCREEN_WIDTH, OLEDScreenConfig::SCREEN_HEIGHT);

// QSPI Flash
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem fatfs;

// NeoPixel
NeoPixelManager neoPixel(PinMap::PIN_STATUS_LED, 1, NEO_GRB + NEO_KHZ800);

// ---------------------------------------------------------------------------
// Hardware Initialization
// ---------------------------------------------------------------------------
void initHardware() {
    // Initialize standard I2C for OLED
    Wire.begin();
    Wire.setClock(OLEDScreenConfig::BUS_SPEED);
    oled.begin(&Wire);

    // Initialize SPI and CAN backend for PSU communication
    SPI.begin();
    psu.begin(&canBackend);

    // Initialize QSPI Flash (raw access)
    flash.begin();

    // Initialize NeoPixel
    neoPixel.begin();
    
    // Register Error Codes
    // Pattern: 0 = Short pulse (300ms), 1 = Long pulse (600ms)
    neoPixel.registerErrorCode((uint8_t)FaultCode::CAN_TIMEOUT,     "10");   // Long-Short
    neoPixel.registerErrorCode((uint8_t)FaultCode::PSU_FAULT,       "11");   // Long-Long
    neoPixel.registerErrorCode((uint8_t)FaultCode::OVER_TEMP_LED,   "010");  // Short-Long-Short
    neoPixel.registerErrorCode((uint8_t)FaultCode::OVER_TEMP_WATER, "011");  // Short-Long-Long
    neoPixel.registerErrorCode((uint8_t)FaultCode::COOLING_FAILURE, "00");   // Short-Short
    neoPixel.registerErrorCode((uint8_t)FaultCode::ENCODER_FAULT,   "001");  // Short-Short-Long
    neoPixel.registerErrorCode((uint8_t)FaultCode::INIT_FAILED,     "1");    // Long

    // Register Init Failure Codes (mapped to boot steps)
    neoPixel.registerErrorCode(1, "0");    // Board Pins (Short)
    neoPixel.registerErrorCode(2, "00");   // Pump (Short-Short)
    neoPixel.registerErrorCode(3, "000");  // Fans (Short-Short-Short)
    neoPixel.registerErrorCode(4, "1");    // PSU (Long)
    neoPixel.registerErrorCode(5, "11");   // Display (Long-Long)

    neoPixel.setState(NeoPixelState::INIT);
}
