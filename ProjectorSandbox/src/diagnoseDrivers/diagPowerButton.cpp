#include "diagPowerButton.h"

void PowerButtonReadOut() {
    Serial.print("Power Button Armed Status: ");
    Serial.println(powerButton.isArmed() ? "ARMED" : "DISARMED");
}