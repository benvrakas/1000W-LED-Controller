// PC-pin RC filter bench test
// ----------------------------
// Standalone firmware (separate PlatformIO env: bench_test) for probing the
// PWM->RC->PSU PC pin filter (AnalogPsuBackend) with a multimeter while the
// board is powered over USB alone. Bypasses SystemController/PsuService and
// the arm sequence entirely -- this talks straight to the driver.
//
// SAFETY: if the PSU's CN71 connector is actually plugged into a live,
// powered UHP-1500-48 while running this, these commands WILL program real
// output current. For a bench test of the filter only, leave CN71
// disconnected (or the PSU unpowered) and probe the PC net directly.
//
// Commands over serial (115200 baud, newline-terminated):
//   f<0.0-1.0>   Set current fraction via AnalogPsuBackend::setCurrentFraction,
//                e.g. "f0.5". Prints the expected post-filter DC voltage.
//   d<0-255>     Bypass the backend's transfer curve and write a raw 8-bit
//                PWM duty directly, e.g. "d128". Useful for characterizing
//                the RC filter itself (e.g. duty 0/128/255 -> 0/1.65/3.3V
//                pre-filter average).
//   0            Force output to zero.
//
// Let the RC filter settle before reading -- check R/C values on the
// schematic for the time constant.

#include <Arduino.h>
#include "drivers/AnalogPsuBackend.h"
#include "config/PinMap.h"
#include "config/PowerConfig.h"

static AnalogPsuBackend pcFilter(PinMap::PIN_PSU_PC_PWM);

static void printExpectedVoltage() {
    // Pulls the target current straight from AnalogPsuBackend::getCommandedCurrentA()
    // instead of re-deriving frac->amps here, so this can't drift out of sync
    // with the driver's actual mapping again.
    const float ratedA = AnalogPsuConfig::PSU_RATED_CURRENT_A;
    const float v20    = AnalogPsuConfig::PC_VOLTAGE_AT_20PCT;
    const float v100   = AnalogPsuConfig::PC_VOLTAGE_AT_100PCT;
    const float vRail  = AnalogPsuConfig::MCU_PWM_RAIL_V;

    float targetA = pcFilter.getCommandedCurrentA();
    if (targetA <= 0.0f) {
        Serial.println(F("  expected PC filter output ~0.000 V"));
        return;
    }

    float ratedFrac = targetA / ratedA;
    float pcVoltage = v20 + (ratedFrac - 0.2f) * (v100 - v20) / 0.8f;
    if (pcVoltage < v20) pcVoltage = v20;
    if (pcVoltage > vRail) pcVoltage = vRail;

    Serial.print(F("  target "));
    Serial.print(targetA, 2);
    Serial.print(F(" A -> expected PC filter output ~"));
    Serial.print(pcVoltage, 3);
    Serial.println(F(" V"));
}

void setup() {
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
        // Wait briefly for the USB CDC host to connect; don't hang forever
        // so the board still does something useful if run unattended.
    }

    pcFilter.begin();
    pcFilter.setEnabled(true); // bench test: always enabled, no arm sequence

    Serial.println(F("PC-pin RC filter bench test"));
    Serial.println(F("Commands: f<0.0-1.0>  d<0-255>  0"));
    Serial.println(F("CAUTION: only safe to sweep freely with CN71 disconnected/PSU unpowered."));
}

void loop() {
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (line == "0") {
        pcFilter.setCurrentFraction(0.0f);
        Serial.println(F("-> fraction 0.0 (output off)"));
        printExpectedVoltage();
        return;
    }

    char kind = line.charAt(0);
    String rest = line.substring(1);

    if (kind == 'f') {
        float frac = rest.toFloat();
        pcFilter.setCurrentFraction(frac);
        Serial.print(F("-> fraction "));
        Serial.println(pcFilter.getCurrentFraction(), 3);
        printExpectedVoltage();
    } else if (kind == 'd') {
        int duty = rest.toInt();
        duty = constrain(duty, 0, 255);
        analogWrite(PinMap::PIN_PSU_PC_PWM, duty);
        Serial.print(F("-> raw duty "));
        Serial.println(duty);
        Serial.print(F("  pre-filter PWM average ~"));
        Serial.print((duty / 255.0f) * AnalogPsuConfig::MCU_PWM_RAIL_V, 3);
        Serial.println(F(" V (ignores backend transfer curve)"));
    } else {
        Serial.println(F("unrecognized command (use f<0-1>, d<0-255>, or 0)"));
    }
}
