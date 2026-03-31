#include "state/StateErrorKill.h"
#include "config/PinMap.h"
#include "drivers/CanBus.h"
#include "logging/FaultManager.h"
#include "core/Hardware.h"
#include <Adafruit_SleepyDog.h>
#include <Arduino.h>

extern CanBusManager psu;

void handleErrorKillState(SystemController &sys, unsigned long now) {
    Watchdog.reset();

    // 0) Keep sensors updating so we can see why we crashed
    sys.context.ledThermistor.updateTemp();
    sys.context.pumpThermistor.updateTemp();
    sys.globalLedTemp = sys.context.ledThermistor.getCelsius();
    sys.globalPumpTemp = sys.context.pumpThermistor.getCelsius();

    static unsigned long lastReportTime = 0;
    const unsigned long reportInterval = 500; 
    bool shouldReport = (now - lastReportTime >= reportInterval) || (lastReportTime == 0);

    if (shouldReport) {
        lastReportTime = now;
        Serial.print(F("--- ERROR_KILL (Step: ")); 
        Serial.print(sys.initData.bootStep);
        Serial.println(F(") ---"));
    }

    // 1) NeoPixel
    FaultCode currentFault = FaultManager::instance().getActiveFault();
    if (currentFault != FaultCode::NONE) {
        if (currentFault == FaultCode::INIT_FAILED) {
            sys.context.neoPixel.setBlinkColor(0x0000FF); 
            sys.context.neoPixel.activateErrorCode(100 + sys.initData.bootStep);
        } else {
            sys.context.neoPixel.activateErrorCode((uint8_t)currentFault);
        }
    }

    // 2) PSU Hardware Kill
    psu.setOperation(false);
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    // 3) UI Reporting
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
        
        Serial.print(F("FAULT ACTIVE: ")); Serial.println(errorMsg);
        Serial.print(F("  LED: ")); Serial.print(sys.globalLedTemp, 1);
        Serial.print(F("C (ADC: ")); Serial.print(sys.context.ledThermistor.getRawADC());
        Serial.print(F(")  Water: ")); Serial.print(sys.globalPumpTemp, 1);
        Serial.print(F("C (ADC: ")); Serial.print(sys.context.pumpThermistor.getRawADC());
        Serial.println(F(")"));

        if (sys.context.oled.isReady()) {
            Adafruit_SH1107* d = sys.context.oled.getDisplay();
            d->clearDisplay();
            d->setTextColor(SH110X_WHITE);
            
            d->setTextSize(2);
            d->setCursor(0, 0);
            d->println(F("ERROR!"));
            
            d->setTextSize(1);
            d->setCursor(0, 20);
            d->println(errorMsg);
            
            d->setCursor(0, 35);
            d->print(F("LED: ")); d->print(sys.globalLedTemp, 1); d->println(F(" C"));
            d->print(F("WTR: ")); d->print(sys.globalPumpTemp, 1); d->println(F(" C"));
            
            d->display();
        }

        Watchdog.reset();
    }
}
