// Aux (lens) fan PWM/tach bench test
// -----------------------------------
// Standalone firmware (separate PlatformIO env: aux_fan_bench) for probing
// the aux fan's PWM output and tach input directly with a multimeter/scope,
// bypassing SystemController, CoolingService, FaultManager, and the arm
// sequence entirely -- no ERROR_KILL latch can interrupt this.
//
// Physical pins (Feather M4 CAN Express silkscreen labels):
//   PWM out : D4  (PA14) -> isolator U6 -> aux fan connector PWM pin
//   Tach in : D13 (PA23) -> isolator U6 -> aux fan connector tach pin
//
// Probe D4 directly at the Feather header for the MCU-side signal, and probe
// at U6's fan-side output pin / the fan connector itself to check whether the
// signal survives the isolator and wiring.
//
// Duty is serial-controlled, frequency is knob-controlled (the front-panel
// rotary encoder) -- turn the knob while probing so both hands stay free for
// the meter/scope. This exists because the installed aux fan turned out to
// be an NMB 12038VA-24Q-EM, not the -24R part production firmware's fixed
// 25kHz PWM config was written against (see docs/System Overview/Hardware.md);
// sweeping frequency while watching RPM response is how we find the real
// number. Temporary bench instrumentation -- not part of production firmware,
// and the PWM-frequency logic below is a bench-local copy of
// TachometerManager::configureFixedFrequencyPwm()/writeDutyToTimer()
// (src/drivers/Tachometers.cpp), not a shared dependency.
//
// Commands over serial (115200 baud, newline-terminated):
//   d<0-255>   Set raw PWM duty directly, e.g. "d128" (~50%). No dead-start
//              floor, no ramping -- whatever you send is written immediately.
//   0          Force duty to 0.
//   1          Force duty to 255 (100%).
//   r          Print current frequency, duty, measured RPM, and raw tach
//              pulse count once. (Also auto-prints every 500ms, and
//              immediately whenever the knob changes the frequency.)
//
// Knob: turn the front-panel rotary encoder to sweep the PWM frequency from
// 5,000-30,000 Hz (spans below and above the NMB-published 25kHz spec for
// the -24R part; starts at 25kHz). The frequency the fan actually throttles
// at -- rather than sitting pinned at full speed regardless of duty -- is
// its real PWM decode frequency.

#include <Arduino.h>
#include "wiring_private.h"
#include "config/PinMap.h"

static volatile uint32_t pulseCount = 0;
static uint16_t currentRPM = 0;
static unsigned long lastRPMCompute = 0;
static uint8_t currentDuty = 0;

static void tachISR() {
    pulseCount++;
}

// --- Encoder (local, self-contained) ----------------------------------
// Doesn't use EncoderManager/HardwareBridges.cpp: aux_fan_bench's
// build_src_filter compiles only this one file, so pulling in the real
// encoder driver would drag in ISR bridges for other drivers (pump/fans/etc)
// this bench deliberately doesn't build, risking link errors. This is a
// minimal re-implementation of the same table-based quadrature decode.
static constexpr int32_t ENC_MIN_COUNTS = 0;
static constexpr int32_t ENC_MAX_COUNTS = 1000;
static constexpr uint32_t FREQ_MIN_HZ = 5000;
static constexpr uint32_t FREQ_MAX_HZ = 30000;
// Maps to 25000Hz -- the previously-assumed -24R spec -- so the sweep starts
// from the known baseline and can be turned outward in either direction.
static constexpr int32_t ENC_START_COUNTS = 800;

static volatile int32_t encoderCounts = ENC_START_COUNTS;
static volatile uint8_t encoderLastState = 0;

static void encoderISR() {
    bool a = digitalRead(PinMap::PIN_ENCODER_A);
    bool b = digitalRead(PinMap::PIN_ENCODER_B);
    uint8_t state = (uint8_t)((a << 1) | b);
    if (state == encoderLastState) return;

    uint8_t combined = (uint8_t)((encoderLastState << 2) | state);
    encoderLastState = state;

    int8_t delta = 0;
    switch (combined) {
        case 0b0001: case 0b0111: case 0b1110: case 0b1000: delta = -1; break;
        case 0b0010: case 0b1011: case 0b1101: case 0b0100: delta = +1; break;
    }

    int32_t next = encoderCounts + delta;
    if (next < ENC_MIN_COUNTS) next = ENC_MIN_COUNTS;
    if (next > ENC_MAX_COUNTS) next = ENC_MAX_COUNTS;
    encoderCounts = next;
}

static uint32_t countsToFreq(int32_t counts) {
    return FREQ_MIN_HZ + (uint32_t)((int64_t)(counts - ENC_MIN_COUNTS) *
        (FREQ_MAX_HZ - FREQ_MIN_HZ) / (ENC_MAX_COUNTS - ENC_MIN_COUNTS));
}

// --- Fixed-frequency PWM on D4 (bench-local copy of the production driver's
// TC-timer configuration -- see file header) ---------------------------
static Tc*     fixedFreqTc = nullptr;
static uint8_t fixedFreqChannel = 0;
static uint8_t fixedFreqPeriod = 0;
static uint32_t currentFreqHz = 0;

static bool configurePwmFrequency(uint32_t freqHz) {
    PinDescription pinDesc = g_APinDescription[PinMap::PIN_AUX_FAN_PWM];
    uint32_t tcNum = GetTCNumber(pinDesc.ulPWMChannel);
    uint8_t tcChannel = GetTCChannelNumber(pinDesc.ulPWMChannel);

    if (tcNum < TCC_INST_NUM) {
        Serial.println(F("PWM pin is TCC-mapped, not TC -- fixed-frequency PWM not supported on this pin"));
        return false;
    }

    static const uint16_t kDivisors[] = {1, 2, 4, 8, 16, 64, 256, 1024};
    static const uint32_t kPrescalerRegs[] = {
        TC_CTRLA_PRESCALER_DIV1,   TC_CTRLA_PRESCALER_DIV2,
        TC_CTRLA_PRESCALER_DIV4,   TC_CTRLA_PRESCALER_DIV8,
        TC_CTRLA_PRESCALER_DIV16,  TC_CTRLA_PRESCALER_DIV64,
        TC_CTRLA_PRESCALER_DIV256, TC_CTRLA_PRESCALER_DIV1024,
    };

    uint32_t bestPeriod = 0;
    uint32_t bestPrescalerReg = 0;
    uint32_t bestErrorHz = 0xFFFFFFFFu;

    for (uint8_t i = 0; i < 8; i++) {
        uint32_t divisor = kDivisors[i];
        uint32_t denom = divisor * freqHz;
        uint32_t rawPeriod = (F_CPU + denom / 2) / denom; // round to nearest
        if (rawPeriod < 1 || rawPeriod > 256) continue;
        uint32_t achievedHz = F_CPU / (divisor * rawPeriod);
        uint32_t errorHz = (achievedHz > freqHz)
            ? (achievedHz - freqHz)
            : (freqHz - achievedHz);
        if (errorHz < bestErrorHz) {
            bestErrorHz = errorHz;
            bestPeriod = rawPeriod;
            bestPrescalerReg = kPrescalerRegs[i];
        }
    }

    if (bestPeriod == 0) {
        Serial.println(F("No valid TC divider for that frequency"));
        return false;
    }

    Tc* TCx = (Tc*) GetTC(pinDesc.ulPWMChannel);

    TCx->COUNT8.CTRLA.bit.ENABLE = 0;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);

    TCx->COUNT8.CTRLA.reg = TC_CTRLA_MODE_COUNT8 | bestPrescalerReg;
    TCx->COUNT8.WAVE.reg = TC_WAVE_WAVEGEN_NPWM;

    while (TCx->COUNT8.SYNCBUSY.bit.PER);
    TCx->COUNT8.PER.reg = (uint8_t)(bestPeriod - 1);

    while (TCx->COUNT8.SYNCBUSY.bit.CC0);
    TCx->COUNT8.CC[tcChannel].reg = 0;

    TCx->COUNT8.CTRLA.bit.ENABLE = 1;
    while (TCx->COUNT8.SYNCBUSY.bit.ENABLE);

    fixedFreqTc = TCx;
    fixedFreqChannel = tcChannel;
    fixedFreqPeriod = (uint8_t)(bestPeriod - 1);
    currentFreqHz = freqHz;
    return true;
}

static void writeDutyToTimer(uint8_t duty) {
    if (fixedFreqTc == nullptr) return;
    // Re-assert the pin mux before every write -- see the identical note in
    // TachometerManager::writeDutyToTimer() (src/drivers/Tachometers.cpp).
    pinPeripheral(PinMap::PIN_AUX_FAN_PWM, PIO_TIMER);
    uint8_t cc = (uint8_t)(((uint32_t)duty * fixedFreqPeriod) / 255);
    while (fixedFreqTc->COUNT8.SYNCBUSY.bit.CC0);
    fixedFreqTc->COUNT8.CC[fixedFreqChannel].reg = cc;
}

static void printStatus() {
    Serial.print(F("freq:"));
    Serial.print(currentFreqHz);
    Serial.print(F("Hz duty:"));
    Serial.print(currentDuty);
    Serial.print(F(" rpm:"));
    Serial.print(currentRPM);
    Serial.print(F(" pulses:"));
    Serial.println(pulseCount);
}

static void setDuty(int duty) {
    duty = constrain(duty, 0, 255);
    currentDuty = (uint8_t)duty;
    writeDutyToTimer(currentDuty);
    Serial.print(F("-> PWM D4 duty = "));
    Serial.println(currentDuty);
}

void setup() {
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
        // Wait briefly for the USB CDC host; don't hang forever if unattended.
    }

    pinMode(PinMap::PIN_AUX_FAN_PWM, OUTPUT);
    analogWriteResolution(8);
    // One-time pin mux / GCLK / timer-enable setup at the core's default
    // prescaler+period; only override it after this has run (see
    // configurePwmFrequency() below), same reasoning as the production
    // driver's begin().
    analogWrite(PinMap::PIN_AUX_FAN_PWM, 0);

    pinMode(PinMap::PIN_AUX_FAN_TACH, INPUT);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_AUX_FAN_TACH), tachISR, FALLING);

    pinMode(PinMap::PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PinMap::PIN_ENCODER_B, INPUT_PULLUP);
    {
        bool a = digitalRead(PinMap::PIN_ENCODER_A);
        bool b = digitalRead(PinMap::PIN_ENCODER_B);
        encoderLastState = (uint8_t)((a << 1) | b);
    }
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PinMap::PIN_ENCODER_B), encoderISR, CHANGE);

    if (!configurePwmFrequency(countsToFreq(encoderCounts))) {
        Serial.println(F("WARNING: falling back to core default PWM frequency (fixed-frequency config failed)"));
    }

    Serial.println(F("Aux fan PWM/tach bench test"));
    Serial.print(F("PWM out: D4 (PA14), pin "));
    Serial.println(PinMap::PIN_AUX_FAN_PWM);
    Serial.print(F("Tach in: D13 (PA23), pin "));
    Serial.println(PinMap::PIN_AUX_FAN_TACH);
    Serial.println(F("Commands: d<0-255>  0  1  r  |  turn knob to sweep PWM frequency 5-30kHz"));
    printStatus();
}

void loop() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            if (line == "0") {
                setDuty(0);
            } else if (line == "1") {
                setDuty(255);
            } else if (line == "r") {
                printStatus();
            } else if (line.charAt(0) == 'd') {
                setDuty(line.substring(1).toInt());
            } else {
                Serial.println(F("unrecognized command (use d<0-255>, 0, 1, or r)"));
            }
        }
    }

    static int32_t lastEncoderCounts = ENC_START_COUNTS;
    noInterrupts();
    int32_t counts = encoderCounts;
    interrupts();
    if (counts != lastEncoderCounts) {
        lastEncoderCounts = counts;
        uint32_t newFreq = countsToFreq(counts);
        if (configurePwmFrequency(newFreq)) {
            writeDutyToTimer(currentDuty); // PER/CTRLA reset above zeroed CC
        }
        printStatus();
    }

    unsigned long now = millis();
    if (now - lastRPMCompute >= 200) {
        unsigned long duration = now - lastRPMCompute;
        noInterrupts();
        uint32_t captured = pulseCount;
        pulseCount = 0;
        interrupts();
        currentRPM = (uint16_t)((captured * 30000UL) / duration);
        lastRPMCompute = now;
    }

    static unsigned long lastPrint = 0;
    if (now - lastPrint >= 500) {
        lastPrint = now;
        printStatus();
    }
}
