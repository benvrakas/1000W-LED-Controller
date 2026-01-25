#include <Adafruit_SleepyDog.h>
#include <Arduino.h>
#include "SystemController.h"
#include "StateInit.h"
#include "StateOnOff.h"
#include "StateErrorKill.h"
#include "OLED.h"
#include "PMBus.h"
#include "Tachometers.h"
#include "Thermistors.h"
#include "Encoder.h"
#include "PowerButton.h"

//Initialize System Controller
SystemController sys;

//Initialize Power Button Manager
PowerButtonManager powerButton(BoardPins::PIN_SW_BTN, BoardPins::PIN_SW_LED);

//Initailize Tachometer Managers
TachometerManager pump(BoardPins::PIN_PUMP_PWM,     BoardPins::PIN_PUMP_TACH);
TachometerManager auxFan(BoardPins::PIN_AUX_FAN_PWM,  BoardPins::PIN_AUX_FAN_TACH);
TachometerManager mainFan(BoardPins::PIN_RAD_FANS_PWM, BoardPins::PIN_RAD_FAN_TACH);
TachometerManager psuFan(BoardPins::PIN_PSU_FAN_PWM,   BoardPins::PIN_PSU_FAN_TACH);

//Initialize Thermistor Managers
ThermistorManager ledThermistor(BoardPins::PIN_THERM_LED, 3950.0f, 10000UL);
ThermistorManager pumpThermistor(BoardPins::PIN_THERM_WATER, 3950.0f, 10000UL);

//Initialize PMBus
PMBusManager psu(BoardPins::PIN_I2C_SDA, BoardPins::PIN_I2C_SCL, PMBusConfig::DEFAULT_ADDRESS);

//Initialize OLED Display
OledManager oled(BoardPins::PIN_I2C_SDA, BoardPins::PIN_I2C_SCL, OLEDScreenConfig::DEFAULT_ADDRESS,
    OLEDScreenConfig::SCREEN_WIDTH,OLEDScreenConfig::SCREEN_HEIGHT);

void setup() {
    Watchdog.enable(1000); //How long??? Probably longer than our Logic Watchdog
    Serial.begin(115200);
    sys.begin();

    //Attach ISRs
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_PUMP_TACH), pumpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_RAD_FAN_TACH), mainFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_AUX_FAN_TACH), auxFanISR,  FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_PSU_FAN_TACH), psuFanISR,  FALLING);
    attachInterrupt(digitalPinToInterrupt(BoardPins::PIN_SW_BTN), powerButtonISR,  FALLING);
}

void loop() {
    sys.update();
    Watchdog.reset();
}