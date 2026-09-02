#pragma once

// Cooling boot trace
// ------------------
// Diagnostic instrumentation for the cooling channels across INIT and the
// first seconds of RUN. Off unless -D COOLING_BOOT_TRACE is set (see the
// commented-out build flag in platformio.ini), and compiles to nothing when
// it isn't -- no Serial traffic, no flash cost in the shipping build.
//
// Prints, per channel, the duty the driver thinks it has commanded, the
// measured RPM, the windowed RPM that stall detection actually judges, and
// the raw ISR pulse count. Also prints PMUXEN for the aux fan's PWM pin
// (PA14): that bit is the difference between the timer driving the pin and
// the pin sitting as dead GPIO while the driver happily updates a compare
// register nothing is connected to. It should read 1 at all times once
// fansInit() has run -- on either boot pass.

#include <Arduino.h>

#ifdef COOLING_BOOT_TRACE

#include "config/PinMap.h"
#include "drivers/Tachometers.h"

inline bool coolingTracePwmMuxEnabled(uint8_t pin) {
    uint32_t port = g_APinDescription[pin].ulPort;
    uint32_t p    = g_APinDescription[pin].ulPin;
    return PORT->Group[port].PINCFG[p].bit.PMUXEN != 0;
}

inline void coolingTraceChannel(const __FlashStringHelper* name, TachometerManager& t) {
    Serial.print(F("  "));
    Serial.print(name);
    Serial.print(F(" duty:"));   Serial.print(t.getDuty());
    Serial.print(F(" rpm:"));    Serial.print(t.getRPM());
    Serial.print(F(" winRpm:")); Serial.print(t.getWindowedRPM());
    Serial.print(F(" pulses:")); Serial.print(t.getPulseCount());
    Serial.print(F(" stall:"));  Serial.println(t.getStallStatus() ? F("Y") : F("N"));
}

// Throttled to `intervalMs`; `tag` labels which phase the sample came from.
#define COOLING_TRACE(sys, now, tag, intervalMs)                               \
    do {                                                                       \
        static unsigned long _ctLast = 0;                                      \
        if ((now) - _ctLast >= (intervalMs) || _ctLast == 0) {                 \
            _ctLast = (now);                                                    \
            Serial.print(F("[TRACE " tag " t="));                              \
            Serial.print(now);                                                 \
            Serial.print(F("ms auxPmuxen:"));                                  \
            Serial.print(coolingTracePwmMuxEnabled(PinMap::PIN_AUX_FAN_PWM));  \
            Serial.println(F("]"));                                            \
            coolingTraceChannel(F("pump"), (sys).context.pump);                \
            coolingTraceChannel(F("main"), (sys).context.mainFan);             \
            coolingTraceChannel(F("psu "), (sys).context.psuFan);              \
            coolingTraceChannel(F("aux "), (sys).context.auxFan);              \
        }                                                                      \
    } while (0)

#else
#define COOLING_TRACE(sys, now, tag, intervalMs) do {} while (0)
#endif
