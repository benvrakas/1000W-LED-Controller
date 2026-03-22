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

    // 0) Activate the NeoPixel error code for the current fault
    FaultCode currentFault = FaultManager::instance().getActiveFault();
    if (currentFault != FaultCode::NONE) {
        if (currentFault == FaultCode::INIT_FAILED) {
            sys.context.neoPixel.setBlinkColor(0x0000FF); // Blue for init fail
            sys.context.neoPixel.activateErrorCode(sys.initData.bootStep);
        } else {
            sys.context.neoPixel.activateErrorCode((uint8_t)currentFault);
        }
    }

    // Update OLED error message
    char errorMsg[32];
    switch (currentFault) {
        case FaultCode::CAN_TIMEOUT:     strcpy(errorMsg, "CAN TIMEOUT"); break;
        case FaultCode::PSU_FAULT:       strcpy(errorMsg, "PSU FAULT"); break;
        case FaultCode::OVER_TEMP_LED:   strcpy(errorMsg, "LED OVERTEMP"); break;
        case FaultCode::OVER_TEMP_WATER: strcpy(errorMsg, "WATER OVERTEMP"); break;
        case FaultCode::COOLING_FAILURE: strcpy(errorMsg, "COOLING FAIL"); break;
        case FaultCode::INIT_FAILED:     sprintf(errorMsg, "INIT FAIL S:%d", sys.initData.bootStep); break;
        default:                         strcpy(errorMsg, "SYSTEM FAULT"); break;
    }
    sys.context.oled.showError(errorMsg);

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
