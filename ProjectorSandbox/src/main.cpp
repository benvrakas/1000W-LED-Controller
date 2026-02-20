#include <Adafruit_SleepyDog.h>
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

//Diagnostic functions includes
#include "diagPowerButton.h"

//Declare objects you want to test
SystemStartup startup;
PowerButtonManager powerButton(BoardPins::PIN_SW_BTN, BoardPins::PIN_SW_LED);

void setup() {
  // put your setup code here, to run once:
  Watchdog.enable(1000); //How long??? Probably longer than our Logic Watchdogs
    Serial.begin(115200);

  startup.boardPinsInit();
  startup.boardPinsVerify(1);
  startup.isrInit();
  powerButton.begin();  
}

void loop() {
  // put your main code here, to run repeatedly:
  PowerButtonReadOut();
  Watchdog.reset();
}
