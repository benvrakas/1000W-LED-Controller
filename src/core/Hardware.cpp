#include "core/Hardware.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
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
}
