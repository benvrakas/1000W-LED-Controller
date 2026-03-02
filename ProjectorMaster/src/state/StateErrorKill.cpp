#include "state/StateErrorKill.h"
#include "util/BoardPins.h"
#include "drivers/CanBus.h"

#include <Arduino.h>

// Global PSU CAN manager instance from main.cpp
extern CanBusManager psu;

// Safe shutdown THEN handle error reporting and logging. Dangerous
// systems can be shut off while keeping control board on to log/report.
void handleErrorKillState(SystemController &sys, unsigned long now) {
    (void)sys;
    (void)now;

    // 1) Ensure PSU output is disabled over CAN
    psu.setOperation(false);

    // 2) Drop all hardware enable lines to the PSU
    pinMode(BoardPins::PIN_PSU_ENABLE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_ENABLE, LOW);

    pinMode(BoardPins::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(BoardPins::PIN_PSU_REMOTE, LOW);

    // Cooling behavior (fans/pump at high speed) will be handled by the
    // CoolingController while the system remains in the ERROR_KILL state.
}
