#include "state/SystemController.h"
#include "state/StateInit.h"
#include "config/PinMap.h"
#include "config/ThermalConfig.h"
#include "logging/FaultManager.h"
#include "core/SystemViewModel.h"
#include "drivers/Tachometers.h" 
#include "drivers/PowerButton.h"
#include "drivers/Encoder.h"
#include "util/CoolingBootTrace.h"

void handleInitState(SystemController &sys, unsigned long currentMillis) {
    auto &data = sys.initData;
    // Lives in InitData, so transitionTo() wipes it on every entry into INIT.
    // It used to be a function-local static, which survived the ERROR_KILL
    // hold-to-clear round trip and made the "full re-validation pass" that
    // path promises silently skip pump/fan/PSU init on the second and later
    // passes -- while boardPinsInit() below still ran and pulled the fan PWM
    // pins back to plain GPIO.
    auto &startup = data.startup;

    if (data.bootStep == 0) {
        data.bootStep = 1;
        data.lastStepTime = currentMillis;
    }
    
    switch (data.bootStep) {
        case 1: //Board pins
            startup.boardPinsInit(sys);
            startup.boardPinsVerify(data.bootStep);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 2; 
            }
            break;
        
        case 2: //Pump
            // Also start the fan spin-up timer here (not just at case 3) so
            // pump and fan verification run concurrently instead of back to
            // back -- fansVerify() in case 3 will typically find its
            // SPINUP_MS window already elapsed by the time we get there.
            startup.pumpInit(sys);
            startup.fansInit(sys);
            startup.pumpVerify(sys, data.bootStep, currentMillis);
            // Keep the fans' RPM/pulse windows fresh while they spin up
            // concurrently with the pump -- fansVerify() (case 3, which
            // does the actual pass/fail check) isn't reached until the pump
            // clears this step, but calculateRPM()'s pulse counter is only
            // ever consumed by .update(), so without this the fans' pulse
            // counts just sit unconsumed for the whole time the pump blocks
            // here, and their RPM never reflects reality.
            startup.fansUpdate(sys, currentMillis);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 3;
            }
            break;

        case 3: //Fans
            startup.fansInit(sys);
            startup.fansVerify(sys, data.bootStep, currentMillis);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 4; 
            }
            break;

        case 4: //PSU
            startup.psuInit(sys);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 5; 
            }
            break;

        case 5: //Display
            startup.displayInit(sys);
            if(startup.getStepStatus(data.bootStep)) {
                data.lastStepTime = currentMillis;
                data.bootStep = 6; 
                data.systemReady = true;
            }
            break;

        case 6: //RUN transition
            if (data.systemReady) {
                sys.transitionTo(SystemState::RUN);
            }
            break;

        default:
            FaultManager::instance().raiseFault(FaultCode::INIT_FAILED);
            sys.context.neoPixel.setBlinkColor(0x0000FF);
            sys.context.neoPixel.activateErrorCode(100 + data.bootStep); 
            sys.transitionTo(SystemState::ERROR_KILL);
            return;
    }

    if (data.bootStep == 2 || data.bootStep == 3) {
        COOLING_TRACE(sys, currentMillis, "INIT", 200UL);
    }

    if (currentMillis - data.lastStepTime > TachometerConfig::BOOT_STEP_TIMEOUT_MS) {
        Serial.print(F("INIT: FATAL - Timeout Step ")); Serial.println(data.bootStep);
        // Steps 2 and 3 fail on tach feedback, and the blink code only
        // identifies the step, not the channel. Name the offenders here so a
        // failed boot is diagnosable off the serial log alone, without
        // needing a COOLING_BOOT_TRACE build.
        startup.reportCoolingStepFailure(sys, data.bootStep);
        COOLING_TRACE(sys, currentMillis, "INIT-TIMEOUT", 0UL);
        FaultManager::instance().raiseFault(FaultCode::INIT_FAILED,
            startup.identifyCoolingFailure(sys, data.bootStep));
        sys.context.neoPixel.setBlinkColor(0x0000FF);
        sys.context.neoPixel.activateErrorCode(100 + data.bootStep); 
        sys.transitionTo(SystemState::ERROR_KILL);
        return;
    }
}

SystemStartup::SystemStartup()
    : _boardPinsReady(false), _pumpReady(false), _fansReady(false),
      _psuReady(false), _displayReady(false), _encoderReady(false),
      _thermistorsReady(false),
      _pumpSpinupStartMs(0), _fansSpinupStartMs(0)
{}

bool SystemStartup::getStepStatus(uint8_t bootStep) const {
    switch (bootStep) {
        case 1:  return _boardPinsReady;
        case 2:  return _pumpReady;
        case 3:  return _fansReady;
        case 4:  return _psuReady;
        case 5:  return _displayReady;
        default: return false; 
    }
}

void SystemStartup::setStepStatus(uint8_t bootStep, bool status) {
    switch (bootStep) {
        case 1: _boardPinsReady = status;    break;
        case 2: _pumpReady = status;         break;
        case 3: _fansReady = status;         break;
        case 4: _psuReady = status;          break;
        case 5: _displayReady = status;      break;
        default: break; 
    }
}

void SystemStartup::boardPinsInit(SystemController& sys) {
    pinMode(PinMap::PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PinMap::PIN_ENCODER_B, INPUT_PULLUP);
    sys.context.encoder.begin();
    pinMode(PinMap::PIN_SW_BTN, INPUT_PULLUP);
    pinMode(PinMap::PIN_SW_LED, OUTPUT);
    digitalWrite(PinMap::PIN_SW_LED, LOW);
    pinMode(PinMap::PIN_THERM_LED, INPUT);
    pinMode(PinMap::PIN_THERM_WATER, INPUT);
    pinMode(PinMap::PIN_RAD_FANS_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_RAD_FANS_PWM, LOW);
    pinMode(PinMap::PIN_PUMP_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_PUMP_PWM, LOW);
    pinMode(PinMap::PIN_PSU_FAN_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_FAN_PWM, LOW);
    pinMode(PinMap::PIN_AUX_FAN_PWM, OUTPUT);
    digitalWrite(PinMap::PIN_AUX_FAN_PWM, LOW);
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
    pinMode(PinMap::PIN_PSU_PC_PWM, OUTPUT);
    analogWrite(PinMap::PIN_PSU_PC_PWM, 0);
    pinMode(PinMap::PIN_RAD_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_PUMP_TACH, INPUT);
    pinMode(PinMap::PIN_PSU_FAN_TACH, INPUT);
    pinMode(PinMap::PIN_AUX_FAN_TACH, INPUT);
    isrInit();
}

void SystemStartup::boardPinsVerify(uint8_t bootStep) {
    bool ok = true;
    int water = analogRead(PinMap::PIN_THERM_WATER);
    int led = analogRead(PinMap::PIN_THERM_LED);
    if (water < 10 || water > 4085) ok = false;
    if (led < 10 || led > 4085) ok = false;
    if (ok) setStepStatus(bootStep, true);
}

bool SystemStartup::isPinSetAsOutput(uint8_t pin) const {
    if (pin >= PINS_COUNT) return false; 
    uint32_t pinMask = 1ul << g_APinDescription[pin].ulPin;
    uint32_t port = g_APinDescription[pin].ulPort;
    return (PORT->Group[port].DIR.reg & pinMask) != 0;
}

bool SystemStartup::isPinSetAsInput(uint8_t pin) const {
    if (pin >= PINS_COUNT) return false; 
    uint32_t pinMask = 1ul << g_APinDescription[pin].ulPin;
    uint32_t port = g_APinDescription[pin].ulPort;
    return (PORT->Group[port].DIR.reg & pinMask) == 0;
}

void SystemStartup::isrInit() {
    detachInterrupt(digitalPinToInterrupt(PinMap::PIN_SW_BTN));
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PUMP_TACH), pumpISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_RAD_FAN_TACH), mainFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_AUX_FAN_TACH), auxFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_PSU_FAN_TACH), psuFanISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_SW_BTN), powerButtonISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_A), encoderAISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_B), encoderBISR, CHANGE);

    NVIC_SetPriority(PinMap::EIC_CHANNEL_BUTTON,        PinMap::EIC_PRIORITY_BUTTON);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_MAIN_FAN_TACH, PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_PUMP_TACH,     PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_PSU_FAN_TACH,  PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_AUX_FAN_TACH,  PinMap::EIC_PRIORITY_TACH);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_ENCODER_A,     PinMap::EIC_PRIORITY_ENCODER);
    NVIC_SetPriority(PinMap::EIC_CHANNEL_ENCODER_B,     PinMap::EIC_PRIORITY_ENCODER);

    // The Arduino core enables the EIC glitch filter (FILTEN=1) with GCLK_EIC sourced
    // from the 32kHz OSCULP32K, making the filter window ~91µs. At 600 PPR fast spin
    // a quarter-cycle can be shorter than that, silently dropping pulses. Disable the
    // filter on just the two encoder channels so edge detection is synchronous.
    EIC->CTRLA.bit.ENABLE = 0;
    while (EIC->SYNCBUSY.bit.ENABLE);
    EIC->CONFIG[0].reg &= ~(EIC_CONFIG_FILTEN0 | EIC_CONFIG_FILTEN1);
    EIC->CTRLA.bit.ENABLE = 1;
    while (EIC->SYNCBUSY.bit.ENABLE);
}

// Both cooling steps follow the same shape: *Init() is idempotent and runs
// every loop iteration of its boot step, arming the spin-up window on the
// first one; *Verify() then holds the step until every channel's tach reads
// above its stall threshold. If a channel never gets there, handleInitState's
// 5s per-step timeout raises INIT_FAILED with the step number -- which is a
// far more diagnosable failure than COOLING_FAILURE two seconds into RUN, and
// keeps the system from ever reaching a state where the LED can be armed on
// unverified cooling.
//
// Note the drivers' setDuty() is used rather than a raw analogWrite(): the
// aux fan runs its timer at a fixed 25kHz with PER=74, so analogWrite()'s
// raw 0-255 value overruns the period register (77 > 74), the compare never
// matches, and the pin sits at a static level instead of the intended duty.
// Going through the driver also keeps _currentDuty/_dutyOnSinceMs consistent
// with the pin, which is what arms the spin-up grace period.

void SystemStartup::pumpInit(SystemController& sys) {
    // Runs once per boot pass, not once per firmware lifetime: _pumpSpinupStartMs
    // is part of InitData and is wiped on every transition into INIT. Guarded
    // because begin() reseeds the RPM compute window -- calling it every loop
    // iteration would restart the window before it could ever close, and
    // getRPM() would sit at 0 forever.
    if (_pumpSpinupStartMs != 0) return;
    sys.context.pump.begin();
    sys.context.pump.setDuty(TachometerConfig::PUMP_SPINUP_DUTY);
    _pumpSpinupStartMs = millis();
}

void SystemStartup::pumpVerify(SystemController& sys, uint8_t bootStep, unsigned long currentMillis) {
    sys.context.pump.update(currentMillis);

    // Ignored channel: skip the tach-verification gate entirely so INIT
    // can't block on (or time out into INIT_FAILED because of) a channel
    // the operator already chose to ignore. pumpInit() above still ran, so
    // the pin/PWM setup happened normally.
    if (sys.globalPumpIgnored) {
        setStepStatus(bootStep, true);
        return;
    }

    // Hold at spin-up duty long enough for a stopped rotor to break loose and
    // come up to speed before the tach reading means anything.
    if (_pumpSpinupStartMs == 0 ||
        (currentMillis - _pumpSpinupStartMs) < TachometerConfig::SPINUP_MS) {
        return;
    }

    if (sys.context.pump.getRPM() > TachometerConfig::PUMP_STALL_RPM) {
        setStepStatus(bootStep, true);
    }
}

void SystemStartup::fansInit(SystemController& sys) {
    // Guarded for the same reason as pumpInit() above.
    if (_fansSpinupStartMs != 0) return;
    sys.context.mainFan.begin();
    sys.context.psuFan.begin();
    sys.context.auxFan.begin();
    sys.context.mainFan.setDuty(TachometerConfig::MAIN_PSU_SPINUP_DUTY);
    sys.context.psuFan.setDuty(TachometerConfig::MAIN_PSU_SPINUP_DUTY);
    sys.context.auxFan.setDuty(TachometerConfig::AUX_SPINUP_DUTY);
    _fansSpinupStartMs = millis();
}

void SystemStartup::fansUpdate(SystemController& sys, unsigned long currentMillis) {
    sys.context.mainFan.update(currentMillis);
    sys.context.psuFan.update(currentMillis);
    sys.context.auxFan.update(currentMillis);
}

void SystemStartup::fansVerify(SystemController& sys, uint8_t bootStep, unsigned long currentMillis) {
    fansUpdate(sys, currentMillis);

    // Ignored channels: unlike pumpVerify() there's no single shared flag to
    // early-out on here -- each of the three fans has its own ignore flag,
    // checked individually below, since one being ignored shouldn't affect
    // the others still being verified normally.
    if (_fansSpinupStartMs == 0 ||
        (currentMillis - _fansSpinupStartMs) < TachometerConfig::SPINUP_MS) {
        return;
    }

    bool ok = true;
    if (!sys.globalMainFanIgnored &&
        sys.context.mainFan.getRPM() <= TachometerConfig::MAIN_PSU_STALL_RPM) ok = false;
    if (TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED && !sys.globalPsuFanIgnored &&
        sys.context.psuFan.getRPM() <= TachometerConfig::MAIN_PSU_STALL_RPM) ok = false;
    if (!sys.globalAuxFanIgnored &&
        sys.context.auxFan.getRPM()  <= TachometerConfig::AUX_STALL_RPM)      ok = false;

    if (ok) setStepStatus(bootStep, true);
}

// Names the channel(s) that failed to reach speed, for boot steps 2 and 3.
// No-op for any other step. Called only on the fatal timeout path, so the
// cost of the Serial traffic doesn't matter.
void SystemStartup::reportCoolingStepFailure(SystemController& sys, uint8_t bootStep) {
    auto report = [](const __FlashStringHelper* name, TachometerManager& t, uint16_t stallRPM) {
        if (t.getRPM() > stallRPM) return;
        Serial.print(F("INIT:   "));
        Serial.print(name);
        Serial.print(F(" failed to reach speed - duty:"));
        Serial.print(t.getDuty());
        Serial.print(F(" rpm:"));
        Serial.print(t.getRPM());
        Serial.print(F(" (need >"));
        Serial.print(stallRPM);
        Serial.println(F(")"));
    };

    if (bootStep == 2) {
        if (!sys.globalPumpIgnored) {
            report(F("PUMP"), sys.context.pump, TachometerConfig::PUMP_STALL_RPM);
        }
    } else if (bootStep == 3) {
        if (!sys.globalMainFanIgnored) {
            report(F("MAIN FAN"), sys.context.mainFan, TachometerConfig::MAIN_PSU_STALL_RPM);
        }
        if (TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED && !sys.globalPsuFanIgnored) {
            report(F("PSU FAN"), sys.context.psuFan, TachometerConfig::MAIN_PSU_STALL_RPM);
        }
        if (!sys.globalAuxFanIgnored) {
            report(F("AUX FAN"),  sys.context.auxFan,  TachometerConfig::AUX_STALL_RPM);
        }
    }
}

// See declaration -- returns the first failing channel's name (matching the
// same convention COOLING_FAILURE's detail already uses: "PUMP"/"MAIN FAN"/
// "PSU FAN"/"AUX FAN"), or nullptr if none is currently below its stall
// threshold for this bootStep. Skips any already-ignored channel so a boot
// timeout can never be misattributed to a channel the operator silenced.
const char* SystemStartup::identifyCoolingFailure(SystemController& sys, uint8_t bootStep) const {
    if (bootStep == 2) {
        if (!sys.globalPumpIgnored && sys.context.pump.getRPM() <= TachometerConfig::PUMP_STALL_RPM) return "PUMP";
    } else if (bootStep == 3) {
        if (!sys.globalMainFanIgnored &&
            sys.context.mainFan.getRPM() <= TachometerConfig::MAIN_PSU_STALL_RPM) return "MAIN FAN";
        if (TachometerConfig::PSU_FAN_TACH_MONITORING_ENABLED && !sys.globalPsuFanIgnored &&
            sys.context.psuFan.getRPM() <= TachometerConfig::MAIN_PSU_STALL_RPM) return "PSU FAN";
        if (!sys.globalAuxFanIgnored &&
            sys.context.auxFan.getRPM() <= TachometerConfig::AUX_STALL_RPM) return "AUX FAN";
    }
    return nullptr;
}

void SystemStartup::psuInit(SystemController& sys) {
    pinMode(PinMap::PIN_PSU_REMOTE, OUTPUT);
    digitalWrite(PinMap::PIN_PSU_REMOTE, LOW);
    setStepStatus(4, true);
}

void SystemStartup::psuVerify(uint8_t bootStep) { setStepStatus(bootStep, true); }

void SystemStartup::displayInit(SystemController& sys) {
    sys.context.oled.showBootScreen("2.0");
    setStepStatus(5, true); 
}

void SystemStartup::displayVerify(uint8_t bootStep) { setStepStatus(bootStep, true); }
