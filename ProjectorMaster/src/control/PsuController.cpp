#include "control/PsuController.h"

PsuController::PsuController() = default;

void PsuController::begin() {
    // TODO: Attach to the global CanBusManager instance, configure initial
    // LED power setpoint, and perform any startup handshakes required by
    // the UHP-1500-48 over CAN.
}

void PsuController::update(SystemController &sys, unsigned long now) {
    (void)sys;
    (void)now;
    // TODO: Pull CAN telemetry, update cached PSU stats (voltage, current,
    // temperature), and publish any derived values (such as LED power
    // fraction) into SystemController or a shared telemetry model.
}

