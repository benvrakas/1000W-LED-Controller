#include "state/StateErrorKill.h"
#include "config/PinMap.h"
#include "drivers/CanBus.h"
#include "logging/ErrorLogger.h"
#include "logging/FaultManager.h"

#include <Arduino.h>

// Global PSU CAN manager instance from main.cpp
extern CanBusManager psu;

// Safe shutdown THEN handle error reporting and logging. Dangerous
// systems can be shut off while keeping control board on to log/report.
void handleErrorKillState(SystemController &sys, unsigned long now) {
    (void)now;

    // 1) Ensure PSU output is disabled over CAN
    psu.setOperation(false);

    // 2) Drop all hardware enable lines to the PSU
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    // 3) Keep ErrorLogger active so it can finish writing any pending 
    // fault to the QSPI flash and maintain the UI/State logging loop
    SystemViewModel vm;
    vm.psuVoltage       = sys.psu.getVoltage();
    vm.psuCurrent       = sys.psu.getCurrent();
    vm.psuPower         = sys.psu.getPower();
    vm.ledTempC         = sys.globalLedTemp;
    vm.waterTempC       = sys.globalPumpTemp;
    vm.mainFanRPM       = sys.globalMainFansRPM;
    vm.auxFanRPM        = sys.globalAuxFanRPM;
    vm.psuFanRPM        = sys.globalPSUFanRPM;
    vm.pumpRPM          = sys.globalPumpRPM;
    vm.knobFraction     = sys.input.getKnobFraction();
    vm.appliedFraction  = sys.psu.getAppliedCurrentFraction();
    vm.isArmed          = sys.input.isArmed();
    
    ErrorLogger::instance().update(sys, vm, now);

    // Cooling behavior (fans/pump at high speed) will be handled by the
    // CoolingController while the system remains in the ERROR_KILL state.
}
