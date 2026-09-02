// All-channel tach bench test
// -----------------------------------
// Standalone firmware (separate PlatformIO env: all_tach_bench) for isolating
// a persistent "0 RPM on every channel, even hand-spun" regression seen in
// ERROR_KILL, after code review across the whole state-machine-integrated
// path (SystemController/FaultManager/CoolingService/PsuService/UiController)
// turned up nothing, and hardware (isolators, header-to-board signal path)
// was independently confirmed good with a jumper and multimeter.
//
// This does the absolute minimum: configure the four tach input pins, attach
// their interrupts, and print raw pulse counts once a second. Nothing else --
// no PWM output, no SystemController, no OLED, no fault logic. It doesn't
// drive the fans/pump at all; spin them by hand (or let them run under
// whatever power they already have) and watch which counters move.
//
// If pulses show up here, the bug is somewhere in the state-machine-integrated
// path and code review needs to keep looking there. If they don't, that's as
// close to hardware-conclusive as this can get without a scope.
//
// Physical pins (Feather M4 CAN Express silkscreen labels):
//   Pump    tach: D11 (PA21)
//   Main    tach: D10 (PA20)
//   PSU fan tach: D12 (PA22)
//   Aux fan tach: D13 (PA23)
//
// Serial output (115200 baud), once per second:
//   pump:<n> main:<n> psu:<n> aux:<n>
// <n> is the pulse count accumulated since the last print, not a cumulative
// total -- a healthy spinning channel should show a nonzero, roughly
// consistent number every second; a stalled/disconnected one should show 0.

#include <Arduino.h>
#include "config/PinMap.h"

static volatile uint32_t pumpPulses = 0;
static volatile uint32_t mainPulses = 0;
static volatile uint32_t psuPulses  = 0;
static volatile uint32_t auxPulses  = 0;

static void pumpTachISR() { pumpPulses++; }
static void mainTachISR() { mainPulses++; }
static void psuTachISR()  { psuPulses++; }
static void auxTachISR()  { auxPulses++; }

void setup() {
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
        // Wait briefly for the USB CDC host; don't hang forever if unattended.
    }

    pinMode(PinMap::PIN_PUMP_TACH, INPUT);
    pinMode(PinMap::PIN_RAD_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_PSU_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_AUX_FAN_TACH, INPUT);

    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PUMP_TACH), pumpTachISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_RAD_FAN_TACH), mainTachISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PSU_FAN_TACH), psuTachISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_AUX_FAN_TACH), auxTachISR, FALLING);

    Serial.println(F("All-channel tach bench test"));
    Serial.println(F("No PWM driven -- spin fans/pump by hand or let them run as-is."));
    Serial.println(F("Printing pulse counts (since last print) once per second: pump main psu aux"));
}

void loop() {
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    if (now - lastPrint < 1000) return;
    lastPrint = now;

    noInterrupts();
    uint32_t p = pumpPulses; pumpPulses = 0;
    uint32_t m = mainPulses; mainPulses = 0;
    uint32_t s = psuPulses;  psuPulses  = 0;
    uint32_t a = auxPulses;  auxPulses  = 0;
    interrupts();

    Serial.print(F("pump:")); Serial.print(p);
    Serial.print(F(" main:")); Serial.print(m);
    Serial.print(F(" psu:"));  Serial.print(s);
    Serial.print(F(" aux:"));  Serial.println(a);
}
