#include "diagPowerButton.h"

void PowerButtonReadOut() {
    Serial.print("Power Button State: ");
    Serial.print(digitalRead(BoardPins::PIN_SW_BTN) == LOW ? "Pressed" : "Released");
    Serial.print("Power Button Armed Status: ");
    Serial.println(powerButton.isArmed());
}