#include <Adafruit_SleepyDog.h>
#include "SystemController.h"
#include "StateInit.h"
#include "StateRun.h"
#include "StateErrorKill.h"
#include "OLED.h"
#include "CanBus.h"
#include "Tachometers.h"
#include "Thermistors.h"
#include "Encoder.h"
#include "PowerButton.h"
#include <Wire.h>

// Secondary I2C bus for the OLED on SERCOM5 (A4/A5, PIO_SERCOM_ALT)
TwoWire oledWire(&sercom5, BoardPins::PIN_OLED_SDA, BoardPins::PIN_OLED_SCL);

void SERCOM5_Handler() {
    oledWire.onService();
}

//Initialize System Controller
SystemController sys;

//Initialize Power Button Manager
PowerButtonManager powerButton(BoardPins::PIN_SW_BTN, BoardPins::PIN_SW_LED);

//Initailize Tachometer Managers, we need a PID class that inherents from Tach class
TachometerManager pump(BoardPins::PIN_PUMP_PWM, BoardPins::PIN_PUMP_TACH,
    TachometerConfig::PUMP_COMPUTE_INTERVAL, TachometerConfig::PUMP_KP, 
    TachometerConfig::PUMP_KI, TachometerConfig::PUMP_KD, TachometerConfig::PUMP_ALPHA, 
    TachometerConfig::PUMP_DEADSTART_DUTY, TachometerConfig::MAX_PUMP_RPM, 
    TachometerConfig::PUMP_STALL_RPM);
TachometerManager mainFan(BoardPins::PIN_RAD_FANS_PWM, BoardPins::PIN_RAD_FAN_TACH,
    TachometerConfig::MAIN_COMPUTE_INTERVAL, TachometerConfig::MAIN_KP, 
    TachometerConfig::MAIN_KI, TachometerConfig::MAIN_KD, TachometerConfig::MAIN_ALPHA, 
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM, 
    TachometerConfig::MAIN_PSU_STALL_RPM);
TachometerManager psuFan(BoardPins::PIN_PSU_FAN_PWM, BoardPins::PIN_PSU_FAN_TACH,
    TachometerConfig::PSU_COMPUTE_INTERVAL, TachometerConfig::PSU_KP, 
    TachometerConfig::PSU_KI, TachometerConfig::PSU_KD, TachometerConfig::PSU_ALPHA, 
    TachometerConfig::MAIN_PSU_DEADSTART_DUTY, TachometerConfig::MAX_MAIN_PSU_RPM, 
    TachometerConfig::MAIN_PSU_STALL_RPM);
TachometerManager auxFan(BoardPins::PIN_AUX_FAN_PWM, BoardPins::PIN_AUX_FAN_TACH,
    TachometerConfig::AUX_COMPUTE_INTERVAL, 0, 0, 0, 0, TachometerConfig::AUX_DEADSTART_DUTY, 
    TachometerConfig::MAX_AUX_RPM, TachometerConfig::AUX_STALL_RPM);

//Initialize Thermistor Managers
ThermistorManager ledThermistor(BoardPins::PIN_THERM_LED, ThermistorConfig::BETA_VALUE_LED, 
    ThermistorConfig::SERIES_RESISTOR_LED);
ThermistorManager pumpThermistor(BoardPins::PIN_THERM_WATER, ThermistorConfig::BETA_VALUE_PUMP, 
    ThermistorConfig::SERIES_RESISTOR_PUMP);

//Initialize CANBus PSU Manager (Mean Well UHP-1500-48)
CanBusManager psu(BoardPins::PIN_CAN_TX, BoardPins::PIN_CAN_RX);

//Initialize OLED Display on secondary I2C bus (A4/A5 via SERCOM5)
OledManager oled(BoardPins::PIN_OLED_SDA, BoardPins::PIN_OLED_SCL, OLEDScreenConfig::DEFAULT_ADDRESS,
    OLEDScreenConfig::SCREEN_WIDTH, OLEDScreenConfig::SCREEN_HEIGHT);

void setup() {
    Watchdog.enable(1000); //How long??? Probably longer than our Logic Watchdogs
    Serial.begin(115200);

    // Bring up the dedicated OLED I2C bus
    oledWire.begin();
    oledWire.setClock(OLEDScreenConfig::BUS_SPEED);
    oled.begin(&oledWire);

    // NOTE: A concrete CAN backend should be created and passed into
    // psu.begin(...) here once a specific CAN library is selected.

    sys.begin();
}

void loop() {
    sys.update();
    Watchdog.reset();
}
