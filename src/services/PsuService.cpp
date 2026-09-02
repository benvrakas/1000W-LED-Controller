#include "services/PsuService.h"

#include <Arduino.h>
#include "drivers/CanBus.h"
#include "drivers/AnalogPsuBackend.h"
#include "config/PinMap.h"
#include "config/PowerConfig.h"

PsuService::PsuService(CanBusManager& psu, AnalogPsuBackend& analog)
    : _psu(psu),
      _analog(analog),
      _uiSetpointFrac(0.0f),
      _appliedCurrentFrac(0.0f),
      _lastUpdateMs(0),
      _slewRatePctPerSec(SLEW_RATE_NORMAL_PCT_PER_SEC),
      _isOn(false),
      _shutdownStartTimeMs(0) {}

void PsuService::begin() {
    _lastUpdateMs = millis();
    _uiSetpointFrac     = 0.0f;
    _appliedCurrentFrac = 0.0f;
    _slewRatePctPerSec  = SLEW_RATE_NORMAL_PCT_PER_SEC;
    _isOn               = false;
    _shutdownStartTimeMs = 0;

    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    if (!PsuControlConfig::PSU_CONTROL_VIA_CAN) {
        _analog.begin();
    }
}

bool PsuService::telemetryValid() const {
    if (PsuControlConfig::PSU_CONTROL_VIA_CAN) {
        return _psu.telemetryValid();
    }
    // Blind mode: there is no feedback path. Report "valid" whenever the
    // PSU is commanded on so UI/fault logic that gates on "comms healthy"
    // doesn't trip.
    return _isOn;
}

void PsuService::requestOn() {
    if (_isOn) return;
    _isOn = true;
    _slewRatePctPerSec = SLEW_RATE_NORMAL_PCT_PER_SEC;
    _shutdownStartTimeMs = millis(); // Reuse this variable as "StartupStartTime" for grace period
}

void PsuService::requestOff() {
    if (!_isOn) return;
    _isOn = false;
    _slewRatePctPerSec = SLEW_RATE_SHUTDOWN_PCT_PER_SEC;
    _shutdownStartTimeMs = millis();
}

float PsuService::applySlew(float currentFrac, float targetFrac, float dtSec) const {
    if (dtSec <= 0.0f) return currentFrac;
    float maxDeltaFracPerSec = _slewRatePctPerSec / 100.0f;
    float maxStep            = maxDeltaFracPerSec * dtSec;
    float error = targetFrac - currentFrac;
    if (error > maxStep) error = maxStep;
    else if (error < -maxStep) error = -maxStep;
    float next = currentFrac + error;
    if (next < 0.0f) next = 0.0f;
    if (next > 1.0f) next = 1.0f;
    return next;
}

void PsuService::update(unsigned long now) {
    uint32_t dtMs = now - _lastUpdateMs;
    float    dtSec = static_cast<float>(dtMs) / 1000.0f;

    _appliedCurrentFrac = applySlew(_appliedCurrentFrac, _uiSetpointFrac, dtSec);
    _lastUpdateMs       = now;

    // Never let the applied fraction reach exactly 0 while armed: frac<=0 is
    // AnalogPsuBackend's dedicated "PC pin fully off" case (0V), and on this
    // PSU that appears to leave it in an unregulated fail-open mode rather
    // than actually dim -- see AnalogPsuConfig::PC_VOLTAGE_MIN_ARMED. Knob-
    // at-minimum must still resolve to a small real current, not "off".
    // Harmless no-op for the CAN path below (0.02A of headroom on a 22A
    // ceiling), so applied unconditionally rather than branching on mode.
    if (_isOn && _appliedCurrentFrac <= 0.0f) {
        _appliedCurrentFrac = 0.001f;
    }

    if (PsuControlConfig::PSU_CONTROL_VIA_CAN) {
        // --- CAN-controlled UHP-1500-48CAN ----------------------------------
        float targetCurrentA = _appliedCurrentFrac * PsuConfig::MAX_LED_CURRENT_A;
        float levelPercent = (targetCurrentA / CanBusConfig::MAX_CURRENT_A) * 100.0f;
        if (levelPercent < 0.0f) levelPercent = 0.0f;
        if (levelPercent > 100.0f) levelPercent = 100.0f;

        _psu.requestPowerPercent(levelPercent);
        _psu.update(now);

        if (_isOn) {
            // Safety: only raise physical enable pin if CAN is responding
            // AND we don't have a sticky fault. No startup grace period --
            // the CAN path is scaffolding for a future CAN/non-CAN PSU
            // toggle, not in active use, so there's no need to paper over a
            // slow-to-wake CAN controller here.
            if (_psu.telemetryValid() && !_psu.hasFault()) {
                _psu.setOperation(true); // Sends 0x80 (ON)
                digitalWrite(PinMap::PIN_PSU_REMOTE, HIGH);
            } else {
                digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
                _psu.setOperation(false);
            }
        } else {
            bool timeoutReached = (now - _shutdownStartTimeMs > 3000);
            if (_appliedCurrentFrac <= 0.01f || timeoutReached) {
                _psu.setOperation(false);
                digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
            }
        }
    } else {
        // --- Analog-controlled UHP-1500-48 (blind mode) ---------------------
        // Drive PC pin to the desired current setpoint via PWM. Remote
        // ON/OFF is driven by arm state alone while armed -- it used to also
        // drop out whenever the applied current was low, which was the
        // actual cause of one arm-time flash: turning the knob up from
        // minimum, the PC pin crosses into "has some voltage" before the
        // applied fraction crossed whatever threshold kept Remote HIGH, so
        // for a moment the PSU saw a PC voltage below its own detection
        // threshold while enabled and free-ran high until the pin caught up.
        // The knob must never gate this pin while armed.
        _analog.setCurrentFraction(_appliedCurrentFrac);

        if (_isOn) {
            // Start driving the PC pin immediately so its RC filter is
            // already charging, but hold Remote LOW for PC_SETTLE_MS first --
            // asserting Remote before the filter has physically settled is a
            // second, independent source of the same arm-time flash: the
            // PSU samples PC mid-transit through the undefined sub-0.4V
            // zone on every fresh arm, not just when the knob is involved.
            // _shutdownStartTimeMs doubles as "time requestOn() was called"
            // here (see its comment in requestOn()).
            _analog.setEnabled(true);
            if (now - _shutdownStartTimeMs >= AnalogPsuConfig::PC_SETTLE_MS) {
                digitalWrite(PinMap::PIN_PSU_REMOTE, HIGH);
            } else {
                // Explicit, not just "skip the HIGH write": if Remote was
                // still HIGH from a moment ago (e.g. disarm hadn't actually
                // pulled it low yet -- see the trace below, which caught
                // exactly this), leaving it alone during the settle window
                // silently defeats the whole delay.
                digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
            }

            // Diagnostic: an intermittent full-power flash on arm has been
            // reported that isn't explained by PC_SETTLE_MS alone (occurs
            // seemingly at random, not tied to quick re-arm). Trace the
            // actual sequencing for a short window after every arm so the
            // next occurrence gives real data instead of another guess.
            // Self-limiting (only fires within ARM_TRACE_MS of requestOn()),
            // so safe to leave in rather than gating behind a build flag.
            {
                static constexpr unsigned long ARM_TRACE_MS = 300UL;
                static constexpr unsigned long ARM_TRACE_INTERVAL_MS = 10UL;
                unsigned long sinceArm = now - _shutdownStartTimeMs;
                if (sinceArm <= ARM_TRACE_MS) {
                    static unsigned long lastArmTraceMs = 0;
                    if (now - lastArmTraceMs >= ARM_TRACE_INTERVAL_MS) {
                        lastArmTraceMs = now;
                        Serial.print(F("[ARM_TRACE t+")); Serial.print(sinceArm);
                        Serial.print(F("ms] appliedFrac:")); Serial.print(_appliedCurrentFrac, 4);
                        Serial.print(F(" uiSetpoint:")); Serial.print(_uiSetpointFrac, 4);
                        Serial.print(F(" commandedA:")); Serial.print(_analog.getCommandedCurrentA(), 2);
                        Serial.print(F(" remote:")); Serial.println(
                            digitalRead(PinMap::PIN_PSU_REMOTE) ? F("HIGH") : F("LOW"));
                    }
                }
            }
        } else {
            // Disarming (power-button-triggered, not knob-triggered): let
            // the shutdown slew (SLEW_RATE_SHUTDOWN_PCT_PER_SEC) actually
            // fade the current down before cutting Remote, rather than
            // yanking output instantly. This only ever runs after _isOn has
            // already gone false -- it doesn't re-evaluate on every knob
            // tick while armed, so it can't reproduce the flash above.
            bool decayedToZero  = (_appliedCurrentFrac <= 0.01f);
            bool timeoutReached = (now - _shutdownStartTimeMs > 3000);
            if (decayedToZero || timeoutReached) {
                _analog.setEnabled(false);
                digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
            }

            // Diagnostic: pairs with ARM_TRACE above -- shows whether Remote
            // actually drops before a quick re-arm, and how long disarm
            // takes to reach decayedToZero. Same self-limiting window/rate.
            {
                static constexpr unsigned long DISARM_TRACE_MS = 1500UL;
                static constexpr unsigned long DISARM_TRACE_INTERVAL_MS = 10UL;
                unsigned long sinceDisarm = now - _shutdownStartTimeMs;
                if (sinceDisarm <= DISARM_TRACE_MS) {
                    static unsigned long lastDisarmTraceMs = 0;
                    if (now - lastDisarmTraceMs >= DISARM_TRACE_INTERVAL_MS) {
                        lastDisarmTraceMs = now;
                        Serial.print(F("[DISARM_TRACE t+")); Serial.print(sinceDisarm);
                        Serial.print(F("ms] appliedFrac:")); Serial.print(_appliedCurrentFrac, 4);
                        Serial.print(F(" decayedToZero:")); Serial.print(decayedToZero ? F("Y") : F("N"));
                        Serial.print(F(" remote:")); Serial.println(
                            digitalRead(PinMap::PIN_PSU_REMOTE) ? F("HIGH") : F("LOW"));
                    }
                }
            }
        }
    }
}

float PsuService::getVoltage() const {
    if (PsuControlConfig::PSU_CONTROL_VIA_CAN) return _psu.getTelemetry().voltage;
    // Blind mode: report nominal rail when on, 0 when off. Real value is
    // unknown without telemetry.
    return _isOn ? CanBusConfig::NOMINAL_VOLTAGE_V : 0.0f;
}

float PsuService::getCurrent() const {
    if (PsuControlConfig::PSU_CONTROL_VIA_CAN) return _psu.getTelemetry().current;
    // Blind mode: report the commanded current, not a measurement. Delegate
    // to the backend so this matches the actual frac->amps mapping it drives
    // onto the PC pin (including the 0%->0A / >0%->20%-100%-rated split).
    return _analog.getCommandedCurrentA();
}

float PsuService::getPower() const {
    if (PsuControlConfig::PSU_CONTROL_VIA_CAN) {
        CanTelemetry t = _psu.getTelemetry();
        return t.voltage * t.current;
    }
    return getVoltage() * getCurrent();
}
