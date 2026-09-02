#include "ui/UiController.h"
#include "drivers/OLED.h"
#include "config/PowerConfig.h"
#include "config/ThermalConfig.h"
#include "logging/FaultManager.h"
#include <string.h>

UiController::UiController(OledManager& oled) : _oled(oled) {}
void UiController::begin() {}

static void drawGauge(Adafruit_SH1107* d, int16_t cx, int16_t cy, int16_t radius, float fraction) {
    if (!d) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    const float startDeg = -120.0f;
    const float endDeg   =  120.0f;
    float sweepDeg       = (endDeg - startDeg) * fraction;
    for (float deg = startDeg; deg <= startDeg + sweepDeg; deg += 6.0f) {
        float rad = deg * 3.14159265f / 180.0f;
        int16_t x = static_cast<int16_t>(cx + radius * cosf(rad));
        int16_t y = static_cast<int16_t>(cy + radius * sinf(rad));
        d->drawPixel(x, y, SH110X_WHITE);
    }
}

void UiController::update(const SystemViewModel& vm, unsigned long now) {
    if (!_oled.isReady()) return;
    Adafruit_SH1107* d = _oled.getDisplay();
    if (!d) return;

    d->clearDisplay();
    d->setTextSize(1);
    d->setTextColor(SH110X_WHITE);
    d->setCursor(0, 0);

    d->print(F("V:"));   d->print(vm.psuVoltage, 1);
    d->print(F(" A:"));  d->print(vm.psuCurrent, 1);
    d->print(F(" P:"));  d->println(vm.psuPower, 0);

    d->setCursor(0, 12);
    d->print(F("LED:"));   d->print(vm.ledTempC, 1);
    d->print(F("C W:"));   d->print(vm.waterTempC, 1);
    d->println(F("C"));

    d->setCursor(0, 24);
    d->print(F("FANS: M:")); d->print(vm.mainFanRPM);
    d->print(F(" A:")); d->println(vm.auxFanRPM);

    d->setCursor(0, 36);
    d->print(F("PUMP: ")); d->print(vm.pumpRPM);
    d->print(F(" PSU: ")); d->println(vm.psuFanRPM);

    d->setCursor(0, 52);
    if (vm.ignoredChannelCount > 0) {
        // Takes priority over the normal arm-status text -- this is the
        // single most urgent thing the operator needs to see.
        d->print(F("[")); d->print(vm.ignoredChannelCount); d->print(F(" IGNORED]"));
    } else if (vm.isArmed) {
        if (PsuControlConfig::PSU_CONTROL_VIA_CAN) {
            // CAN mode: trust voltage telemetry to confirm the PSU is talking.
            if (vm.psuVoltage < 1.0f) {
                d->print(F("[NO PSU COMM!]"));
            } else {
                d->print(F("[POWER ON]"));
            }
        } else {
            // Blind mode: no telemetry, but commanded current is shown above.
            d->print(F("[ON-BLIND]"));
        }
    } else {
        d->print(F("[IDLE]"));
    }

    const int16_t centerX = 110;
    const int16_t radius  = 12;
    drawGauge(d, centerX, 18, radius, vm.knobFraction);
    d->setCursor(centerX - 3, 32); d->print(F("K"));
    drawGauge(d, centerX, 48, radius, vm.appliedFraction);
    d->setCursor(centerX - 3, 62); d->print(F("L"));

    d->display();
}

// Duty is stored as a raw 0-255 PWM value; shown as 0-100 percent since
// that's what's actually meaningful to read at a glance.
static uint8_t dutyPercent(uint8_t duty) {
    return (uint16_t)duty * 100 / 255;
}

static const char* faultName(FaultCode code) {
    switch (code) {
        case FaultCode::CAN_TIMEOUT:     return "CAN TIMEOUT";
        case FaultCode::PSU_FAULT:       return "PSU FAULT";
        case FaultCode::OVER_TEMP_LED:   return "LED OVERTEMP";
        case FaultCode::OVER_TEMP_WATER: return "WATER OVERTEMP";
        case FaultCode::COOLING_FAILURE: return "COOLING FAIL";
        case FaultCode::ENCODER_FAULT:   return "ENCODER FAULT";
        case FaultCode::INIT_FAILED:     return "INIT FAILED";
        default:                         return "SYSTEM FAULT";
    }
}

static void drawDiagnosticsFooter(Adafruit_SH1107* d, uint8_t pageIndex) {
    d->setTextSize(1);
    d->setCursor(0, 56);
    d->print(F("<PAGE "));
    d->print(pageIndex + 1);
    d->print(F("/"));
    d->print(UiController::DIAGNOSTICS_PAGE_COUNT);
    d->print(F(">"));
}

void UiController::renderFaultDiagnostics(FaultCode active, const char* detail,
                                           const FaultSnapshot& snap,
                                           const CoolingLiveReadout& live,
                                           const IgnoredChannels& ignored,
                                           uint8_t pageIndex, unsigned long now) {
    (void)now;
    if (!_oled.isReady()) return;
    Adafruit_SH1107* d = _oled.getDisplay();
    if (!d) return;

    d->clearDisplay();
    d->setTextColor(SH110X_WHITE);
    d->setTextWrap(false); // clip rather than corrupt the layout if a line runs long

    if (pageIndex == 0) {
        d->setTextSize(2);
        d->setCursor(0, 0);
        d->println(F("ERROR!"));

        d->setTextSize(1);
        d->setCursor(0, 18);
        d->println(faultName(active));

        d->setCursor(0, 27);
        if (detail) {
            d->print(F("-> "));
            d->println(detail);
        }

        d->setCursor(0, 45);
        d->println(F("TURN KNOB ->"));
    } else {
        bool outOfRange = false;
        bool isIgnored = false;
        IgnorableSource src = identifyFaultSource(active, detail);

        d->setTextSize(1);
        d->setCursor(0, 0);

        switch (pageIndex) {
            case 1: // LED Temp
                d->println(F("LED TEMP"));
                d->setCursor(0, 16);
                d->print(snap.ledTempC, 1); d->println(F("C"));
                d->setCursor(0, 26);
                d->print(F("LIMIT:")); d->print(ThermistorConfig::MAX_TEMP_LED, 1); d->println(F("C"));
                outOfRange = snap.ledTempC > ThermistorConfig::MAX_TEMP_LED;
                isIgnored = ignored.ledTemp;
                break;

            case 2: // Water Temp
                d->println(F("WATER TEMP"));
                d->setCursor(0, 16);
                d->print(snap.waterTempC, 1); d->println(F("C"));
                d->setCursor(0, 26);
                d->print(F("LIMIT:")); d->print(ThermistorConfig::MAX_TEMP_PUMP, 1); d->println(F("C"));
                outOfRange = snap.waterTempC > ThermistorConfig::MAX_TEMP_PUMP;
                isIgnored = ignored.waterTemp;
                break;

            case 3: // Pump
                d->println(F("PUMP"));
                d->setCursor(0, 16);
                d->print(live.pumpRPM); d->print(F(" RPM/")); d->print(dutyPercent(live.pumpDuty)); d->println(F("%"));
                d->setCursor(0, 26);
                d->print(F("STALL<")); d->print(TachometerConfig::PUMP_STALL_RPM);
                d->print(F(" PK:")); d->println(live.pumpPeakRPM);
                // Peak, not the live reading, is the verdict: ERROR_KILL's
                // failsafe zeroes duty (and so RPM) for non-overheat faults,
                // which would otherwise make every channel look "stalled"
                // after the fact regardless of how well it actually spun up.
                outOfRange = live.pumpPeakRPM <= TachometerConfig::PUMP_STALL_RPM;
                isIgnored = ignored.pump;
                break;

            case 4: // Main Fan
                d->println(F("MAIN FAN"));
                d->setCursor(0, 16);
                d->print(live.mainFanRPM); d->print(F(" RPM/")); d->print(dutyPercent(live.mainFanDuty)); d->println(F("%"));
                d->setCursor(0, 26);
                d->print(F("STALL<")); d->print(TachometerConfig::MAIN_PSU_STALL_RPM);
                d->print(F(" PK:")); d->println(live.mainFanPeakRPM);
                outOfRange = live.mainFanPeakRPM <= TachometerConfig::MAIN_PSU_STALL_RPM;
                isIgnored = ignored.mainFan;
                break;

            case 5: // PSU Fan
                d->println(F("PSU FAN"));
                d->setCursor(0, 16);
                d->print(live.psuFanRPM); d->print(F(" RPM/")); d->print(dutyPercent(live.psuFanDuty)); d->println(F("%"));
                d->setCursor(0, 26);
                if (!TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED) {
                    // Compile-time disable takes precedence over the
                    // runtime ignore flag in the message shown, since it's
                    // the more permanent of the two reasons this channel
                    // isn't being checked.
                    d->println(F("MONITORING OFF"));
                } else {
                    d->print(F("STALL<")); d->print(TachometerConfig::MAIN_PSU_STALL_RPM);
                    d->print(F(" PK:")); d->println(live.psuFanPeakRPM);
                    outOfRange = live.psuFanPeakRPM <= TachometerConfig::MAIN_PSU_STALL_RPM;
                }
                isIgnored = ignored.psuFan;
                break;

            case 6: // Aux Fan
                d->println(F("AUX FAN"));
                d->setCursor(0, 16);
                d->print(live.auxFanRPM); d->print(F(" RPM/")); d->print(dutyPercent(live.auxFanDuty)); d->println(F("%"));
                d->setCursor(0, 26);
                d->print(F("STALL<")); d->print(TachometerConfig::AUX_STALL_RPM);
                d->print(F(" PK:")); d->println(live.auxFanPeakRPM);
                outOfRange = live.auxFanPeakRPM <= TachometerConfig::AUX_STALL_RPM;
                isIgnored = ignored.auxFan;
                break;

            case 7: // PSU Comms
                d->println(F("PSU COMMS"));
                d->setCursor(0, 16);
                if (PsuControlConfig::PSU_CONTROL_VIA_CAN) {
                    d->print(F("FAULT:")); d->println(snap.canFaultFlag ? F("YES") : F("NO"));
                    d->setCursor(0, 26);
                    d->print(F("TELEM OK:")); d->println(snap.canTelemetryValid ? F("YES") : F("NO"));
                    outOfRange = snap.canFaultFlag && snap.canTelemetryValid;
                } else {
                    d->println(F("ANALOG MODE"));
                    d->setCursor(0, 26);
                    d->println(F("N/A"));
                }
                isIgnored = ignored.psuComms;
                break;

            case 8: // Encoder
                d->println(F("ENCODER"));
                d->setCursor(0, 16);
                d->print(snap.encoderIllegalAccum); d->print(F("/")); d->println(FaultManager::ENCODER_FAULT_THRESHOLD);
                d->setCursor(0, 26);
                d->println(F("ILLEGAL/WINDOW"));
                outOfRange = snap.encoderIllegalAccum >= FaultManager::ENCODER_FAULT_THRESHOLD;
                isIgnored = ignored.encoder;
                break;

            default:
                d->println(F("--"));
                break;
        }

        // primaryCause and isIgnored can never both be true in practice --
        // ignoring a channel clears its fault immediately, so a page can't
        // still be "the cause" of whatever's currently latched once it's on
        // the ignore list -- but check cause first regardless, since that's
        // the more important thing to surface if it somehow did happen.
        bool primaryCause = (pageIndex == 1 && src == IgnorableSource::LED_TEMP) ||
                             (pageIndex == 2 && src == IgnorableSource::WATER_TEMP) ||
                             (pageIndex == 3 && src == IgnorableSource::PUMP) ||
                             (pageIndex == 4 && src == IgnorableSource::MAIN_FAN) ||
                             (pageIndex == 5 && src == IgnorableSource::PSU_FAN) ||
                             (pageIndex == 6 && src == IgnorableSource::AUX_FAN) ||
                             (pageIndex == 7 && src == IgnorableSource::PSU_COMMS) ||
                             (pageIndex == 8 && src == IgnorableSource::ENCODER);

        d->setCursor(0, 40);
        if (primaryCause) {
            d->println(F("<<CAUSED SHUTDOWN"));
        } else if (isIgnored) {
            d->println(F("IGNORED"));
        } else if (outOfRange) {
            d->println(F("OUT OF RANGE"));
        } else {
            d->println(F("OK"));
        }
    }

    drawDiagnosticsFooter(d, pageIndex);
    d->display();
}
