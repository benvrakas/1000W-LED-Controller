#include "state/StateRun.h"
#include "services/CoolingService.h"
#include "services/PsuService.h"
#include "services/InputService.h"
#include "ui/UiController.h"
#include "logging/FaultManager.h"

SystemRunning::SystemRunning() {}
void SystemRunning::begin() {}

void handleRunState(SystemController &sys, unsigned long now) {
    static bool initialized = false;

    if (!initialized) {
        Serial.println(F("StateRun: Starting..."));
        sys.cooling.begin();
        sys.psu.begin();
        sys.input.begin();
        sys.ui.begin();
        initialized = true;
    }

    sys.input.update(now);
    bool armed = sys.input.isArmed();

    if (sys.input.edgeArmedOn()) {
        if (!sys.psu.telemetryValid()) {
            Serial.println(F("PSU: Starting 5s CAN Discovery Grace Period..."));
        }
        Serial.println(F("PSU: Arming Sequence..."));
        sys.input.forceKnobToZero();
        sys.psu.requestOn();
        // Use shutdownStartTimeMs as an 'ArmingStartTime' for the grace period
        // (This is a bit of a hack to reuse the existing timer variable)
    }

    if (sys.input.edgeArmedOff()) {
        Serial.println(F("PSU: Disarming Sequence..."));
        sys.input.forceKnobToZero();
        sys.psu.requestOff();
    }

    // Only allow knob to affect PSU when armed
    if (armed) {
        sys.psu.setUiSetpointFraction(sys.input.getKnobFraction());
    } else {
        sys.psu.setUiSetpointFraction(0.0f);
    }
    
    sys.psu.update(now);
    sys.cooling.update(now);

    const CoolingState& cs = sys.cooling.getState();
    sys.globalLedTemp     = cs.ledTempC;
    sys.globalPumpTemp    = cs.waterTempC;
    sys.globalMainFansRPM = cs.mainFanRPM;
    sys.globalAuxFanRPM   = cs.auxFanRPM;
    sys.globalPSUFanRPM   = cs.psuFanRPM;
    sys.globalPumpRPM     = cs.pumpRPM;

    SystemViewModel vm;
    vm.psuVoltage       = sys.psu.getVoltage();
    vm.psuCurrent       = sys.psu.getCurrent();
    vm.psuPower         = sys.psu.getPower();
    vm.ledTempC         = cs.ledTempC;
    vm.waterTempC       = cs.waterTempC;
    vm.mainFanRPM       = cs.mainFanRPM;
    vm.auxFanRPM        = cs.auxFanRPM;
    vm.psuFanRPM        = cs.psuFanRPM;
    vm.pumpRPM          = cs.pumpRPM;
    vm.knobFraction     = sys.input.getKnobFraction();
    vm.appliedFraction  = sys.psu.getAppliedCurrentFraction();
    vm.isArmed          = armed;

    FaultManager::instance().update(sys, now);
    sys.input.setButtonLed(armed);

    static unsigned long lastUiUpdate = 0;
    if (now - lastUiUpdate >= 100) {
        sys.ui.update(vm, now);
        lastUiUpdate = now;
    }
}
