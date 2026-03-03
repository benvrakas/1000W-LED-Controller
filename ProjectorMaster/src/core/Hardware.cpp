#include "core/Hardware.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
#include <SPI.h>

// ---------------------------------------------------------------------------
// Global Hardware Instantiation
// ---------------------------------------------------------------------------

// Secondary I2C bus for the OLED on SERCOM5 (A4/A5, PIO_SERCOM_ALT)
TwoWire oledWire(&sercom5, BoardPins::PIN_OLED_SDA, BoardPins::PIN_OLED_SCL);

// SERCOM5 Interrupt Handler
void SERCOM5_Handler() {
    oledWire.onService();
}

// Power Button
PowerButtonManager powerButton(BoardPins::PIN_SW_BTN, BoardPins::PIN_SW_LED);

// Tachometers (Fans & Pump)
TachometerManager pump(BoardPins::PIN_PUMP_PWM, BoardPins::PIN_PUMP_TACH,
    TachometerConfig::PUMP_DEADSTART_DUTY, TachometerConfig::MAX_PUMP_RPM,
    TachometerConfig::PUMP_STALL_RPM);

TachometerManager mainFan(BoardPins::PIN_RAD_FANS_PWM, BoardPins::PIN_RAD_FAN_TACH,
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM,
    TachometerConfig::MAIN_PSU_STALL_RPM);

TachometerManager psuFan(BoardPins::PIN_PSU_FAN_PWM, BoardPins::PIN_PSU_FAN_TACH,
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM,
    TachometerConfig::MAIN_PSU_STALL_RPM);

TachometerManager auxFan(BoardPins::PIN_AUX_FAN_PWM, BoardPins::PIN_AUX_FAN_TACH,
    TachometerConfig::AUX_DEADSTART_DUTY, TachometerConfig::MAX_AUX_RPM,
    TachometerConfig::AUX_STALL_RPM);

// Thermistors
ThermistorManager ledThermistor(BoardPins::PIN_THERM_LED, ThermistorConfig::BETA_VALUE_LED, 
    ThermistorConfig::SERIES_RESISTOR_LED);

ThermistorManager pumpThermistor(BoardPins::PIN_THERM_WATER, ThermistorConfig::BETA_VALUE_PUMP, 
    ThermistorConfig::SERIES_RESISTOR_PUMP);

// CAN Bus & PSU
CanBusManager psu(BoardPins::PIN_CAN_TX, BoardPins::PIN_CAN_RX);
Mcp2515CanBackend canBackend(BoardPins::PIN_CAN_CS, 16);

// OLED Display
OledManager oled(BoardPins::PIN_OLED_SDA, BoardPins::PIN_OLED_SCL, OLEDScreenConfig::DEFAULT_ADDRESS,
    OLEDScreenConfig::SCREEN_WIDTH, OLEDScreenConfig::SCREEN_HEIGHT);

// QSPI Flash
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem fatfs;

// ---------------------------------------------------------------------------
// Hardware Initialization
// ---------------------------------------------------------------------------
void initHardware() {
    // Bring up the dedicated OLED I2C bus
    oledWire.begin();
    oledWire.setClock(OLEDScreenConfig::BUS_SPEED);
    oled.begin(&oledWire);

    // Initialize SPI and CAN backend for PSU communication
    SPI.begin();
    psu.begin(&canBackend);

    // Initialize QSPI Flash (raw access)
    flash.begin();
}
