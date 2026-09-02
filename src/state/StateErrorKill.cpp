#include "state/StateErrorKill.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
#include "drivers/CanBus.h"
#include "logging/FaultManager.h"
#include "core/Hardware.h"
#include <Adafruit_SleepyDog.h>
#include <Arduino.h>

extern CanBusManager psu;

// Duty is stored as a raw 0-255 PWM value; the OLED/serial telemetry shows
// it as a 0-100 percent instead since that's what's actually meaningful to
// read at a glance.
static uint8_t dutyPercent(uint8_t duty) {
    return (uint16_t)duty * 100 / 255;
}

// --- Diagnostics screen navigation (encoder + button) -----------------
// Quantized to 8 clicks per revolution: the knob reports
// EncoderManager::COUNTS_PER_REV (600) quadrature counts/rev, so one
// navigation "click" = 600/8 = 75 counts. Deriving it from the existing
// constant (rather than a tuned magic number) keeps it correct if the
// encoder hardware ever changes.
static constexpr int16_t DIAG_STEPS_PER_REV   = 8;
static constexpr int16_t DIAG_COUNTS_PER_STEP = EncoderManager::COUNTS_PER_REV / DIAG_STEPS_PER_REV; // 75
static constexpr unsigned long BTN_DEBOUNCE_MS = 30; // minimum hold to count as a real click, not contact noise

static uint8_t wrapPageIndex(int32_t steps, uint8_t numPages) {
    int32_t p = steps % (int32_t)numPages;
    if (p < 0) p += numPages;
    return (uint8_t)p;
}

void handleErrorKillState(SystemController &sys, unsigned long now) {
    Watchdog.reset();

    // 0) Keep sensors updating so we can see why we crashed
    sys.context.ledThermistor.updateTemp();
    sys.context.pumpThermistor.updateTemp();
    sys.globalLedTemp = sys.context.ledThermistor.getCelsius();
    sys.globalPumpTemp = sys.context.pumpThermistor.getCelsius();

    // 0b) Diagnostics screen navigation (encoder + button). Read directly
    // off the drivers rather than through InputService/PowerButtonManager's
    // update() -- neither runs in ERROR_KILL, and that's fine: their state
    // machine is about arm/disarm, which is irrelevant here since the PSU is
    // unconditionally hard-killed below regardless of arm state. Navigation
    // is purely local, cosmetic state.
    ErrorKillData &nav = sys.errorData;
    if (!nav.navBaselineSet) {
        nav.navBaselineCounts = sys.context.encoder.getCounts();
        nav.navBaselineSet = true;
    }

    // Short button press resets navigation back to the summary page --
    // avoids having to spin all the way back after drilling into a page.
    bool btnDown = sys.context.powerButton.isPressed();
    if (btnDown && !nav.btnWasPressed) {
        nav.btnPressStartMs = now;
    } else if (!btnDown && nav.btnWasPressed) {
        if (now - nav.btnPressStartMs >= BTN_DEBOUNCE_MS) {
            nav.navBaselineCounts = sys.context.encoder.getCounts();
        }
        nav.ignoreBlockedReported = false;
    }
    nav.btnWasPressed = btnDown;

    int16_t navDelta = sys.context.encoder.getCounts() - nav.navBaselineCounts;
    nav.currentPage = wrapPageIndex(navDelta / DIAG_COUNTS_PER_STEP, UiController::DIAGNOSTICS_PAGE_COUNT);

    static unsigned long lastReportTime = 0;
    const unsigned long reportInterval = 500; 
    bool shouldReport = (now - lastReportTime >= reportInterval) || (lastReportTime == 0);

    if (shouldReport) {
        lastReportTime = now;
        Serial.print(F("--- ERROR_KILL [t="));
        Serial.print(now);
        Serial.print(F("ms] (Step: "));
        Serial.print(sys.initData.bootStep);
        Serial.println(F(") ---"));
    }

    // 1) NeoPixel
    FaultCode currentFault = FaultManager::instance().getActiveFault();
    const char* faultDetail = FaultManager::instance().getFaultDetail();
    if (currentFault != FaultCode::NONE) {
        if (currentFault == FaultCode::INIT_FAILED) {
            sys.context.neoPixel.setBlinkColor(0x0000FF); 
            sys.context.neoPixel.activateErrorCode(100 + sys.initData.bootStep);
        } else {
            sys.context.neoPixel.activateErrorCode((uint8_t)currentFault);
        }
    }

    // 1b) Long-press-to-ignore: 3s hold clears the latched fault and adds
    // *only the channel that actually caused it* to the per-session ignore
    // list (see SystemController.h), so a misbehaving channel can be
    // silenced permanently (until a physical power cycle) while every other
    // channel stays fully monitored. identifyFaultSource() is the same
    // classifier the diagnostics screen uses to mark "<<CAUSED SHUTDOWN", so
    // the two always agree on which channel is responsible.
    if (btnDown && (now - nav.btnPressStartMs >= 3000UL)) {
        IgnorableSource src = identifyFaultSource(currentFault, faultDetail);
        switch (src) {
            case IgnorableSource::LED_TEMP:   sys.globalLedTempIgnored   = true; break;
            case IgnorableSource::WATER_TEMP: sys.globalWaterTempIgnored = true; break;
            case IgnorableSource::PUMP:       sys.globalPumpIgnored      = true; break;
            case IgnorableSource::MAIN_FAN:   sys.globalMainFanIgnored   = true; break;
            case IgnorableSource::PSU_FAN:    sys.globalPsuFanIgnored    = true; break;
            case IgnorableSource::AUX_FAN:    sys.globalAuxFanIgnored    = true; break;
            case IgnorableSource::PSU_COMMS:  sys.globalPsuCommsIgnored  = true; break;
            case IgnorableSource::ENCODER:    sys.globalEncoderIgnored   = true; break;
            case IgnorableSource::NONE:       break;
        }

        if (src != IgnorableSource::NONE) {
            Serial.print(F("ERROR_KILL: 3s hold - ignoring channel ("));
            Serial.print(faultDetail ? faultDetail : "n/a");
            Serial.println(F(") for the rest of this power cycle"));
            FaultManager::instance().clearFault(currentFault);
            sys.transitionTo(SystemState::INIT);
            return;
        } else if (!nav.ignoreBlockedReported) {
            // No channel to attribute this to (e.g. a board-pins INIT
            // sanity failure) -- nothing to ignore, stays latched. Reported
            // once per hold rather than every tick while held.
            nav.ignoreBlockedReported = true;
            Serial.println(F("ERROR_KILL: 3s hold - this fault has no ignorable channel, still latched"));
        }
    }

    // 2) PSU Hardware Kill
    psu.setOperation(false);
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);

    // 2b) Cooling failsafe: an overheat fault means the danger is heat we
    // already have, so keep every fan/pump at max to dissipate it. Any other
    // fault has no such justification for keeping them spinning -- and for
    // COOLING_FAILURE specifically, the offending channel is already
    // stalled/faulted -- so drive everything to 0%. This also replaces
    // whatever duty CoolingService last wrote before the fault fired, which
    // would otherwise stay latched on these PWM pins indefinitely (they're
    // never touched by ERROR_KILL) even after the PSU kill above.
    bool overheat = (currentFault == FaultCode::OVER_TEMP_LED) ||
                    (currentFault == FaultCode::OVER_TEMP_WATER);
    if (overheat) {
        sys.context.mainFan.setDuty(FanCurveConfig::MAIN_DUTY_MAX);
        sys.context.psuFan.setDuty(FanCurveConfig::MAIN_DUTY_MAX);
        sys.context.auxFan.setDuty(FanCurveConfig::AUX_DUTY_MAX);
        sys.context.pump.setDuty(FanCurveConfig::PUMP_DUTY_MAX);
    } else {
        sys.context.mainFan.stop();
        sys.context.psuFan.stop();
        sys.context.auxFan.stop();
        sys.context.pump.stop();
    }

    // Keep RPM feedback advancing while latched in ERROR_KILL -- otherwise
    // _currentRPM freezes at whatever it read the instant the fault fired,
    // and the diagnostics screen below would show a stale number forever
    // even though the setDuty()/stop() calls above are still actively
    // driving these channels.
    sys.context.mainFan.update(now);
    sys.context.psuFan.update(now);
    sys.context.auxFan.update(now);
    sys.context.pump.update(now);

    // The Serial/OLED diagnostics below read sys.globalXxxDuty/RPM, which
    // are otherwise only refreshed by StateRun -- without this they'd keep
    // showing whatever was active at the moment the fault fired, making it
    // look like the setDuty()/stop()/update() calls above did nothing.
    sys.globalMainFanDuty = sys.context.mainFan.getDuty();
    sys.globalPSUFanDuty  = sys.context.psuFan.getDuty();
    sys.globalAuxFanDuty  = sys.context.auxFan.getDuty();
    sys.globalPumpDuty    = sys.context.pump.getDuty();
    sys.globalMainFansRPM = sys.context.mainFan.getRPM();
    sys.globalPSUFanRPM   = sys.context.psuFan.getRPM();
    sys.globalAuxFanRPM   = sys.context.auxFan.getRPM();
    sys.globalPumpRPM     = sys.context.pump.getRPM();

    // 3) Serial Reporting
    if (shouldReport) {
        char errorMsg[32];
        switch (currentFault) {
            case FaultCode::CAN_TIMEOUT:     strncpy(errorMsg, "CAN TIMEOUT", 31); break;
            case FaultCode::PSU_FAULT:       strncpy(errorMsg, "PSU FAULT", 31); break;
            case FaultCode::OVER_TEMP_LED:   strncpy(errorMsg, "LED OVERTEMP", 31); break;
            case FaultCode::OVER_TEMP_WATER: strncpy(errorMsg, "WATER OVERTEMP", 31); break;
            case FaultCode::COOLING_FAILURE: strncpy(errorMsg, "COOLING FAIL", 31); break;
            case FaultCode::ENCODER_FAULT:   strncpy(errorMsg, "ENCODER FAULT", 31); break;
            case FaultCode::INIT_FAILED:     snprintf(errorMsg, 31, "INIT FAIL S:%d", sys.initData.bootStep); break;
            default:                         strncpy(errorMsg, "SYSTEM FAULT", 31); break;
        }

        Serial.print(F("FAULT ACTIVE: ")); Serial.print(errorMsg);
        if (faultDetail) { Serial.print(F(" (")); Serial.print(faultDetail); Serial.print(F(")")); }
        Serial.println();
        Serial.print(F("  LED: ")); Serial.print(sys.globalLedTemp, 1);
        Serial.print(F("C (ADC: ")); Serial.print(sys.context.ledThermistor.getRawADC());
        Serial.print(F(")  Water: ")); Serial.print(sys.globalPumpTemp, 1);
        Serial.print(F("C (ADC: ")); Serial.print(sys.context.pumpThermistor.getRawADC());
        Serial.println(F(")"));
        Serial.print(F("  Main: ")); Serial.print(sys.globalMainFansRPM);
        Serial.print(F("rpm/")); Serial.print(dutyPercent(sys.globalMainFanDuty)); Serial.print(F("%"));
        Serial.print(F("  Aux: "));  Serial.print(sys.globalAuxFanRPM);
        Serial.print(F("rpm/")); Serial.print(dutyPercent(sys.globalAuxFanDuty));  Serial.print(F("%"));
        Serial.print(F("  PSU: "));  Serial.print(sys.globalPSUFanRPM);
        Serial.print(F("rpm/")); Serial.print(dutyPercent(sys.globalPSUFanDuty));  Serial.print(F("%"));
        Serial.print(F("  Pump: ")); Serial.print(sys.globalPumpRPM);
        Serial.print(F("rpm/")); Serial.print(dutyPercent(sys.globalPumpDuty));    Serial.println(F("%"));
        Serial.print(F("  Pulses (since last 200ms window) - Main:"));
        Serial.print(sys.context.mainFan.getPulseCount());
        Serial.print(F(" Aux:"));  Serial.print(sys.context.auxFan.getPulseCount());
        Serial.print(F(" PSU:"));  Serial.print(sys.context.psuFan.getPulseCount());
        Serial.print(F(" Pump:")); Serial.println(sys.context.pump.getPulseCount());

        const FaultSnapshot &snap = FaultManager::instance().getSnapshot();
        Serial.print(F("  CAN Fault: ")); Serial.print(snap.canFaultFlag ? F("YES") : F("NO"));
        Serial.print(F("  Telem Valid: ")); Serial.print(snap.canTelemetryValid ? F("YES") : F("NO"));
        Serial.print(F("  Encoder Illegal: ")); Serial.print(snap.encoderIllegalAccum);
        Serial.print(F("/")); Serial.println(FaultManager::ENCODER_FAULT_THRESHOLD);

        Watchdog.reset();
    }

    // 4) OLED Diagnostics Screen. Refreshed on its own faster cadence,
    // decoupled from the 500ms Serial-report throttle above, so turning the
    // knob or pressing the button feels responsive rather than laggy
    // (matches the ~100ms cadence StateRun.cpp uses for its own UI update).
    static unsigned long lastUiTime = 0;
    const unsigned long uiInterval = 100;
    if ((now - lastUiTime >= uiInterval) || (lastUiTime == 0)) {
        lastUiTime = now;
        CoolingLiveReadout live{
            sys.globalPumpRPM, sys.globalMainFansRPM, sys.globalPSUFanRPM, sys.globalAuxFanRPM,
            sys.globalPumpDuty, sys.globalMainFanDuty, sys.globalPSUFanDuty, sys.globalAuxFanDuty,
            sys.context.pump.getPeakRPM(), sys.context.mainFan.getPeakRPM(),
            sys.context.psuFan.getPeakRPM(), sys.context.auxFan.getPeakRPM()
        };
        IgnoredChannels ignored{
            sys.globalLedTempIgnored, sys.globalWaterTempIgnored, sys.globalPumpIgnored,
            sys.globalMainFanIgnored, sys.globalPsuFanIgnored, sys.globalAuxFanIgnored,
            sys.globalPsuCommsIgnored, sys.globalEncoderIgnored
        };
        sys.ui.renderFaultDiagnostics(currentFault, faultDetail,
                                       FaultManager::instance().getSnapshot(), live, ignored,
                                       nav.currentPage, now);
    }
}
