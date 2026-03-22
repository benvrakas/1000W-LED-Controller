#include "state/StateErrorKill.h"
#include "config/PinMap.h"
#include "drivers/CanBus.h"
#include "logging/ErrorLogger.h"
#include "logging/FaultManager.h"
#include "core/Hardware.h"
#include <Adafruit_SleepyDog.h>
#include <Arduino.h>

extern CanBusManager psu;

void handleErrorKillState(SystemController &sys, unsigned long now) {
    Watchdog.reset();

    static unsigned long lastReportTime = 0;
    const unsigned long reportInterval = 500; 
    bool shouldReport = (now - lastReportTime >= reportInterval) || (lastReportTime == 0);

    if (shouldReport) {
        lastReportTime = now;
        Serial.print(F("--- ERROR_KILL (Step: ")); 
        Serial.print(sys.initData.bootStep);
        Serial.println(F(") ---"));
    }

    // 0) NeoPixel
    FaultCode currentFault = FaultManager::instance().getActiveFault();
    if (currentFault != FaultCode::NONE) {
        if (currentFault == FaultCode::INIT_FAILED) {
            sys.context.neoPixel.setBlinkColor(0x0000FF); 
            sys.context.neoPixel.activateErrorCode(100 + sys.initData.bootStep);
        } else {
            sys.context.neoPixel.activateErrorCode((uint8_t)currentFault);
        }
    }

    // 1) PSU Hardware Kill
    psu.setOperation(false);
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    // 2) UI & Logging
    if (shouldReport) {
        char errorMsg[32];
        switch (currentFault) {
            case FaultCode::CAN_TIMEOUT:     strncpy(errorMsg, "CAN TIMEOUT", 31); break;
            case FaultCode::PSU_FAULT:       strncpy(errorMsg, "PSU FAULT", 31); break;
            case FaultCode::OVER_TEMP_LED:   strncpy(errorMsg, "LED OVERTEMP", 31); break;
            case FaultCode::OVER_TEMP_WATER: strncpy(errorMsg, "WATER OVERTEMP", 31); break;
            case FaultCode::COOLING_FAILURE: strncpy(errorMsg, "COOLING FAIL", 31); break;
            case FaultCode::INIT_FAILED:     snprintf(errorMsg, 31, "INIT FAIL S:%d", sys.initData.bootStep); break;
            default:                         strncpy(errorMsg, "SYSTEM FAULT", 31); break;
        }
        
        if (sys.context.oled.isReady()) {
            sys.context.oled.showError(errorMsg);
        }

        SystemViewModel vm = {};
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
        Watchdog.reset();
    }
}
